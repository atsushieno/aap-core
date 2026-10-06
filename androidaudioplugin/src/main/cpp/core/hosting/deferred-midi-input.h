#pragma once

#include "midi2-port-buffer.h"
#include <algorithm>
#include <array>
#include <vector>

namespace aap::internal {
// One port, owned by the processing caller. Construct only while processing is
// stopped. Four port payloads (at most 256 KiB) bound retention during control
// suspension. Overflow drops the backlog and schedules pedal release, MIDI
// channel reset and explicit note-offs before accepting another note into DSP.
class DeferredMidiInput {
    std::vector<uint32_t> storage;
    size_t head{0}, used{0};
    std::array<uint16_t, 32> channels{}; // MIDI1/MIDI2 x group, channels seen by DSP
    std::array<std::array<uint32_t, 4>, 512> notes{};
    bool recovering{false};
    unsigned recovery_channel{0}, recovery_stage{0}, recovery_note{0};
    uint32_t overflows{0};

    static bool timing(uint32_t word) noexcept {
        auto status = (word >> 16) & 0xF0;
        return (word >> 28) == 0 && (status == CMIDI2_UTILITY_STATUS_JR_CLOCK ||
                status == CMIDI2_UTILITY_STATUS_JR_TIMESTAMP || status == CMIDI2_UTILITY_STATUS_DELTA_CLOCKSTAMP);
    }
    void overflow() noexcept {
        head = used = 0;
        ++overflows;
        // Do not restart an ongoing recovery on repeated overflow: otherwise
        // a busy input could indefinitely prevent later channels' note-offs.
        if (!recovering) {
            recovering = true;
            recovery_channel = recovery_stage = recovery_note = 0;
        }
    }
    bool recover(AAPMidiBufferHeader& header, size_t capacity) noexcept {
        auto* output = reinterpret_cast<uint8_t*>(&header + 1);
        while (recovery_channel < 512) {
            auto bank = recovery_channel / 16, channel = recovery_channel % 16;
            if (!(channels[bank] & (1u << channel))) {
                ++recovery_channel;
                continue;
            }
            auto size = bank < 16 ? 4u : 8u;
            if (header.length + size > capacity) return false;
            uint32_t packet[2]{};
            auto prefix = ((bank < 16 ? 2u : 4u) << 28) | ((bank % 16) << 24) | (channel << 16);
            if (recovery_stage < 4) {
                constexpr unsigned controls[]{64, 66, 120, 123};
                packet[0] = prefix | 0xB00000 | (controls[recovery_stage++] << 8);
            } else {
                while (recovery_note < 128 && !(notes[recovery_channel][recovery_note / 32] & (1u << (recovery_note % 32))))
                    ++recovery_note;
                if (recovery_note == 128) {
                    ++recovery_channel;
                    recovery_stage = recovery_note = 0;
                    continue;
                }
                packet[0] = prefix | 0x800000 | (recovery_note++ << 8);
            }
            memcpy(output + header.length, packet, size);
            header.length += size;
        }
        recovering = false;
        return true;
    }
public:
    explicit DeferredMidiInput(size_t payloadCapacity)
        : storage(std::min(payloadCapacity, size_t{64 * 1024}) * 4 / sizeof(uint32_t)) {}

    bool pending() const noexcept { return used || recovering; }
    uint32_t overflowCount() const noexcept { return overflows; }
    size_t capacityBytes() const noexcept { return storage.size() * sizeof(uint32_t); }

    void reset() noexcept {
        head = used = 0;
        recovering = false;
        channels.fill(0);
        for (auto& row : notes) row.fill(0);
    }
    // Observe what is actually handed to DSP, not notes that were merely queued.
    void observe(const AAPMidiBufferHeader& header) noexcept {
        auto* input = reinterpret_cast<const uint8_t*>(&header + 1);
        for (size_t offset = 0; offset < header.length;) {
            auto word = cmidi2_ump_read_uint32_bytes(input + offset);
            auto type = word >> 28;
            if (type == 2 || type == 4) {
                auto bank = ((type == 4 ? 16u : 0u) + ((word >> 24) & 15));
                auto channel = (word >> 16) & 15;
                channels[bank] |= 1u << channel;
                if (((word >> 20) & 15) == 9) {
                    auto key = (word >> 8) & 127;
                    notes[bank * 16 + channel][key / 32] |= 1u << (key % 32);
                }
            }
            offset += cmidi2_ump_get_num_bytes(word);
        }
    }
    // Input has already been validated and stripped of AAPXS. Deferred events
    // are late: remove their old clock/delta packets and replay at block start.
    void capture(AAPMidiBufferHeader& header, bool late = true) noexcept {
        auto* input = reinterpret_cast<const uint8_t*>(&header + 1);
        for (size_t offset = 0; offset < header.length;) {
            auto word = cmidi2_ump_read_uint32_bytes(input + offset);
            auto words = static_cast<size_t>(cmidi2_ump_get_num_bytes(word)) / 4;
            if (!late || !timing(word)) {
                if (words > storage.size() - used) { overflow(); break; }
                for (size_t i = 0; i < words; ++i) {
                    uint32_t value;
                    memcpy(&value, input + offset + i * 4, 4);
                    storage[(head + used + i) % storage.size()] = value;
                }
                used += words;
            }
            offset += words * 4;
        }
        header.length = 0;
    }
    // A fresh block appended while draining may have remained queued. Only its
    // timing is now stale; compact in place before appending this block's input.
    void age() noexcept {
        size_t destination = 0;
        for (size_t source = 0; source < used;) {
            auto word = storage[(head + source) % storage.size()];
            auto words = static_cast<size_t>(cmidi2_ump_get_num_bytes(word)) / 4;
            if (!timing(word)) {
                for (size_t i = 0; i < words; ++i)
                    storage[(head + destination + i) % storage.size()] = storage[(head + source + i) % storage.size()];
                destination += words;
            }
            source += words;
        }
        used = destination;
    }
    void drain(AAPMidiBufferHeader& header, size_t capacity) noexcept {
        if (recovering && !recover(header, capacity)) return;
        auto* output = reinterpret_cast<uint8_t*>(&header + 1);
        while (used) {
            auto words = static_cast<size_t>(cmidi2_ump_get_num_bytes(storage[head])) / 4;
            if (header.length + words * 4 > capacity) break;
            for (size_t i = 0; i < words; ++i)
                memcpy(output + header.length + i * 4, &storage[(head + i) % storage.size()], 4);
            head = (head + words) % storage.size();
            used -= words;
            header.length += words * 4;
        }
    }
};
}
