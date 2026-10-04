#pragma once
#include <atomic>
#include <chrono>
#include <deque>
#include <functional>
#include <mutex>
#include "aapxs-transport.h"

namespace aap::internal {
// Older plugins also enqueue MIDI replies for Binder requests. Wait for a later
// completed audio block before issuing another request, across all extensions.
// Processing only increments an atomic and wakes the existing eventfd worker.
class LegacyAAPXSSender {
    static_assert(std::atomic<uint64_t>::is_always_lock_free, "audio progress must not use an atomic runtime lock");
    using Clock = std::chrono::steady_clock;
    struct Entry {
        AAPXSRequestContext request;
        AAPXSBinderChannel::Transmit transmit;
        Clock::time_point deadline;
    };
    std::mutex mutex;
    std::recursive_mutex delivery;
    std::deque<Entry> queue;
    std::atomic<uint64_t> generation{0};
    std::atomic<bool> waiting{false};
    uint64_t consumed{0}; // delivery gate
    bool closed{false}; // queue mutex
    std::atomic<bool> cancelling{false};
    unsigned cancellation_depth{0}; // delivery gate
    static void fail(const Entry& entry, const char* error, void* target) {
        if (entry.request.error_callback)
            entry.request.error_callback(entry.request.callback_user_data, target, error);
        else if (entry.request.callback)
            entry.request.callback(entry.request.callback_user_data, target);
    }
public:
    class Cancellation {
        LegacyAAPXSSender* sender;
        std::unique_lock<std::recursive_mutex> gate;
    public:
        explicit Cancellation(LegacyAAPXSSender& owner) : sender(&owner), gate(owner.delivery) {
            ++owner.cancellation_depth;
            owner.cancelling.store(true, std::memory_order_release);
        }
        Cancellation(Cancellation&& other) noexcept : sender(other.sender), gate(std::move(other.gate)) { other.sender = nullptr; }
        ~Cancellation() { unlock(); }
        void unlock() {
            if (!sender) return;
            if (--sender->cancellation_depth == 0) sender->cancelling.store(false, std::memory_order_release);
            sender = nullptr; gate.unlock();
        }
    };
    bool enqueue(const AAPXSRequestContext& request, AAPXSBinderChannel::Transmit transmit,
                 int timeoutMs = 1000) {
        std::lock_guard<std::mutex> lock(mutex);
        if (closed || cancelling.load(std::memory_order_acquire) || queue.size() >= 255) return false;
        queue.push_back({request, std::move(transmit), Clock::now() + std::chrono::milliseconds(timeoutMs)});
        waiting.store(true, std::memory_order_release);
        return true;
    }
    bool processingCompleted() noexcept {
        generation.fetch_add(1, std::memory_order_release);
        return waiting.load(std::memory_order_acquire);
    }
    Clock::time_point nextDeadline() {
        std::lock_guard<std::mutex> lock(mutex);
        return queue.empty() ? Clock::time_point::max() : queue.front().deadline;
    }
    void poll(bool active, void* target) {
        for (unsigned n = 0; n < 255; ++n) {
            std::unique_lock<std::recursive_mutex> gate(delivery);
            Entry entry;
            bool expired;
            {
                std::lock_guard<std::mutex> lock(mutex);
                if (queue.empty() || closed) { waiting.store(false, std::memory_order_release); return; }
                expired = queue.front().deadline <= Clock::now();
                if (!expired && active && generation.load(std::memory_order_acquire) == consumed) return;
                entry = std::move(queue.front()); queue.pop_front();
            }
            // Once claimed, this is an in-flight transmission. Do not hold an
            // application gate across Binder: its callback may cancel this instance.
            gate.unlock();
            if (expired) fail(entry, "legacy AAPXS waiting for audio progress timed out", target);
            else {
                if (!entry.transmit(entry.request)) fail(entry, "request could not be sent", target);
                // Read after IPC returns: a block concurrent with transmission may
                // have finished before the old service enqueued its redundant reply.
                consumed = generation.load(std::memory_order_acquire);
                if (active) return;
            }
        }
    }
    // Caller retains the gate across channel abort, preventing another queued
    // transmission from being claimed. Already claimed IPC can complete late.
    Cancellation cancel(const char* error, void* target, bool close = false) {
        Cancellation gate(*this);
        std::deque<Entry> dropped;
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (close) closed = true;
            dropped.swap(queue);
            waiting.store(false, std::memory_order_release);
        }
        for (auto& entry : dropped) fail(entry, error, target);
        return gate;
    }
};
}
