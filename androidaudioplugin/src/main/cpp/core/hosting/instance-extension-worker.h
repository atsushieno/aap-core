#ifndef AAP_CORE_INSTANCE_EXTENSION_WORKER_H
#define AAP_CORE_INSTANCE_EXTENSION_WORKER_H

#include <atomic>
#include <functional>
#include <thread>
#include "event-wakeup.h"

namespace aap::internal {
// start/stop belong to the control lifecycle. Work publication explicitly wakes
// this worker; it blocks until an event or the earliest pending request deadline.
// Stop/join before releasing targets; descriptors remain alive until destruction.
class InstanceExtensionWorker {
    std::atomic<bool> stopping{false};
    std::thread thread;
    EventWakeup wakeup;
public:
    ~InstanceExtensionWorker() { stop(); }
    void start(std::function<void()> dispatch,
               std::function<EventWakeup::Deadline()> nextDeadline = [] { return EventWakeup::Deadline::max(); }) {
        if (thread.joinable()) return;
        stopping.store(false);
        thread = std::thread([this, dispatch = std::move(dispatch), nextDeadline = std::move(nextDeadline)] {
            while (!stopping.load(std::memory_order_acquire)) {
                wakeup.consume();
                dispatch();
                if (!stopping.load(std::memory_order_acquire)) wakeup.wait(nextDeadline());
            }
        });
    }
    bool isCurrentThread() const { return thread.joinable() && thread.get_id() == std::this_thread::get_id(); }
    bool isStopping() const { return stopping.load(std::memory_order_acquire); }
    void notify() noexcept { wakeup.notify(); }
    void requestStop() { stopping.store(true, std::memory_order_release); notify(); }
    void stop() {
        requestStop();
        if (isCurrentThread()) return; // a control/lifecycle caller will join after this dispatch
        if (thread.joinable()) thread.join();
    }
};
}
#endif
