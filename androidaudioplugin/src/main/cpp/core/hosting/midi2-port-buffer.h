#ifndef AAP_CORE_HOSTING_MIDI2_PORT_BUFFER_H
#define AAP_CORE_HOSTING_MIDI2_PORT_BUFFER_H

#include "aap/android-audio-plugin.h"
#include "aap/ext/midi.h"
#include "../include_cmidi2.h"

namespace aap::internal {

inline bool isCompleteUmpSequence(const void* data, size_t size) {
    if (!data || size == 0) return false;
    auto* bytes = static_cast<const uint8_t*>(data);
    size_t offset = 0;
    while (offset < size) {
        if (size - offset < sizeof(uint32_t)) return false;
        auto packetSize = cmidi2_ump_get_num_bytes(cmidi2_ump_read_uint32_bytes(bytes + offset));
        if (packetSize == 0 || static_cast<size_t>(packetSize) > size - offset) return false;
        offset += packetSize;
    }
    return true;
}

// Returns the MIDI2 buffer of `portIndex`, whose length is written by the other process: it is
// trimmed to the last complete UMP within the buffer, so that it can be iterated safely. RT-safe.
inline AAPMidiBufferHeader* getMidi2PortBuffer(aap_buffer_t* buffer, int32_t portIndex) {
    auto mbh = (AAPMidiBufferHeader*) buffer->get_buffer(buffer, portIndex);
    if (!mbh)
        return nullptr;
    auto size = buffer->get_buffer_size(buffer, portIndex);
    uint32_t capacity = size > (int32_t) sizeof(AAPMidiBufferHeader) ? size - sizeof(AAPMidiBufferHeader) : 0;
    uint32_t length = mbh->length < capacity ? mbh->length : capacity;
    auto data = (uint8_t*) (mbh + 1);
    uint32_t valid = 0;
    while (valid + sizeof(uint32_t) <= length) {
        auto umpSize = cmidi2_ump_get_num_bytes(cmidi2_ump_read_uint32_bytes(data + valid));
        if (valid + umpSize > length)
            break;
        valid += umpSize;
    }
    mbh->length = valid;
    return mbh;
}

}

#endif //AAP_CORE_HOSTING_MIDI2_PORT_BUFFER_H
