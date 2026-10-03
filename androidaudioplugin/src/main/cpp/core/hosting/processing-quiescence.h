#ifndef AAP_CORE_PROCESSING_QUIESCENCE_H
#define AAP_CORE_PROCESSING_QUIESCENCE_H

#include <atomic>
#include <mutex>
#include "event-wakeup.h"

namespace aap::internal {
// Control calls serialize with each other and wait off the processing thread.
// Processing only announces activity and checks suspension; it never acquires a
// lock, spins, or waits. The last leaving block may signal eventfd. Sequential
// consistency closes the entry/suspension race.
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
public:
    class Process {
        ProcessingQuiescence& owner;
        bool entered;
    public:
        explicit Process(ProcessingQuiescence& state) noexcept : owner(state) {
            owner.active.fetch_add(1);
            entered = !owner.suspended.load();
            if (!entered) owner.leaveProcessing();
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
                owner.suspended.store(true);
                owner.waiting_for_idle.store(true);
                while (owner.active.load() != 0) {
                    owner.dsp_idle.wait();
                    owner.dsp_idle.consume();
                }
                owner.waiting_for_idle.store(false);
            }
        }
        ~Control() { if (--owner.control_depth == 0) owner.suspended.store(false); }
    };
};
}
#endif
