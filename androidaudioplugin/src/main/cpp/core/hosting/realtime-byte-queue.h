#ifndef AAP_CORE_REALTIME_BYTE_QUEUE_H
#define AAP_CORE_REALTIME_BYTE_QUEUE_H

#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <memory>
#include <limits>

namespace aap::internal {
// Bounded MPSC queue, with one consumer. All storage is allocated in the constructor.
// A producer makes at most four reservation attempts, then reports backpressure. A
// stalled producer can delay consumption of its slot, but cannot make the consumer wait.
// Producers/consumer must have stopped before destruction. Never reset a live queue.
template<uint32_t Slots>
class RealtimeByteQueue {
    static_assert(Slots > 1 && (Slots & (Slots - 1)) == 0);
    static_assert(std::atomic<uint32_t>::is_always_lock_free);
    struct Slot {
        std::atomic<uint32_t> sequence{0};
        size_t size{0};
    };
    std::array<Slot, Slots> slots{};
    const size_t capacity;
    const size_t stride;
    std::unique_ptr<uint64_t[]> storage;
    alignas(64) std::atomic<uint32_t> enqueue_position{0};
    alignas(64) uint32_t dequeue_position{0}; // consumer only
    std::atomic<uint32_t> rejected{0};
public:
    explicit RealtimeByteQueue(size_t bytesPerSlot)
        : capacity(bytesPerSlot), stride((bytesPerSlot + 7) / 8),
          storage(new uint64_t[stride * Slots]{}) {
        for (uint32_t i = 0; i < Slots; ++i)
            slots[i].sequence.store(i, std::memory_order_relaxed);
    }

    bool tryPush(const void* data, size_t size, const void* prefix = nullptr, size_t prefixSize = 0) noexcept {
        if (size > capacity || prefixSize > capacity - size || (size && !data) || (prefixSize && !prefix)) {
            rejected.fetch_add(1, std::memory_order_relaxed);
            return false;
        }
        auto position = enqueue_position.load(std::memory_order_relaxed);
        for (unsigned attempt = 0; attempt < 4; ++attempt) {
            auto& slot = slots[position & (Slots - 1)];
            auto sequence = slot.sequence.load(std::memory_order_acquire);
            if (static_cast<int32_t>(sequence - position) < 0)
                break;
            if (sequence != position) {
                position = enqueue_position.load(std::memory_order_relaxed);
                continue;
            }
            if (!enqueue_position.compare_exchange_strong(position, position + 1, std::memory_order_relaxed))
                continue;
            auto* destination = reinterpret_cast<uint8_t*>(storage.get() + (position & (Slots - 1)) * stride);
            if (prefixSize) memcpy(destination, prefix, prefixSize);
            if (size) memcpy(destination + prefixSize, data, size);
            slot.size = prefixSize + size;
            slot.sequence.store(position + 1, std::memory_order_release);
            return true;
        }
        rejected.fetch_add(1, std::memory_order_relaxed);
        return false;
    }

    // Return false from consume to retain the head for a later block. Borrowed data
    // must not escape consume. The consumer never spins on an unpublished slot.
    template<class Consume>
    bool tryConsume(Consume&& consume) {
        auto& slot = slots[dequeue_position & (Slots - 1)];
        if (slot.sequence.load(std::memory_order_acquire) != dequeue_position + 1)
            return false;
        auto* data = storage.get() + (dequeue_position & (Slots - 1)) * stride;
        if (!consume(data, slot.size))
            return false;
        slot.sequence.store(dequeue_position + Slots, std::memory_order_release);
        ++dequeue_position;
        return true;
    }

    uint32_t rejectedCount() const noexcept { return rejected.load(std::memory_order_relaxed); }
};
}
#endif
