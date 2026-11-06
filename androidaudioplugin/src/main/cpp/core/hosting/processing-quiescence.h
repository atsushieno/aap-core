#ifndef AAP_CORE_PROCESSING_QUIESCENCE_H
#define AAP_CORE_PROCESSING_QUIESCENCE_H

#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>
#include "event-wakeup.h"

namespace aap::internal {
// Control calls serialize with each other and wait off the processing thread.
// Realtime processing only announces activity and checks suspension: no lock, spin or wait.
// The last leaving block may signal eventfd. Sequential consistency closes the entry race.
class ProcessingQuiescence {
    static_assert(std::atomic<unsigned>::is_always_lock_free);
    static_assert(std::atomic<bool>::is_always_lock_free);
    std::atomic<unsigned> active{0};
    std::atomic<bool> suspended{false};
    std::recursive_mutex control_mutex;
    EventWakeup dsp_idle;
    std::atomic<bool> waiting_for_idle{false};
    void leaveProcessing() noexcept {
        if (active.fetch_sub(1) == 1 && waiting_for_idle.load()) dsp_idle.notify();
    }
    unsigned control_depth{0}; // protected by control_mutex; permits nested control calls
    std::atomic<std::thread::id> control_thread{};
public:
    class Process {
        ProcessingQuiescence& owner;
        bool entered;
    public:
        explicit Process(ProcessingQuiescence& state) noexcept
                : Process(state, false, [] { return false; }) {}
        // Offline blocks wait for control work instead of being skipped, without counting as
        // active, as control work waits for that. They still give up if `shouldStop` says so,
        // or if control work on this very thread could never finish while waiting.
        template<typename ShouldStop>
        Process(ProcessingQuiescence& state, bool waitForControl, ShouldStop shouldStop) noexcept : owner(state) {
            while (true) {
                owner.active.fetch_add(1);
                entered = !owner.suspended.load();
                if (entered) return;
                owner.leaveProcessing();
                if (!waitForControl || shouldStop() || owner.control_thread.load() == std::this_thread::get_id())
                    return;
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
        }
        ~Process() { if (entered) owner.leaveProcessing(); }
        explicit operator bool() const noexcept { return entered; }
    };
    class Control {
        ProcessingQuiescence& owner;
        std::unique_lock<std::recursive_mutex> lock;
    public:
        explicit Control(ProcessingQuiescence& state) : owner(state), lock(state.control_mutex) {
            if (owner.control_depth++ == 0) {
                owner.control_thread.store(std::this_thread::get_id());
                owner.suspended.store(true);
                owner.waiting_for_idle.store(true);
                while (owner.active.load() != 0) {
                    owner.dsp_idle.wait();
                    owner.dsp_idle.consume();
                }
                owner.waiting_for_idle.store(false);
            }
        }
        ~Control() {
            if (--owner.control_depth == 0) {
                owner.suspended.store(false);
                owner.control_thread.store(std::thread::id{});
            }
        }
    };
};
}
#endif
