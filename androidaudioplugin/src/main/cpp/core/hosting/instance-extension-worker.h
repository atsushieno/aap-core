#ifndef AAP_CORE_INSTANCE_EXTENSION_WORKER_H
#define AAP_CORE_INSTANCE_EXTENSION_WORKER_H

#include <atomic>
#include <chrono>
#include <functional>
#include <thread>

namespace aap::internal {
// start/stop belong to the instance's control lifecycle. No wakeup syscall or
// lazy initialization is performed by processing: the worker polls every 1 ms.
// Stop and join before releasing any handler targets or request buffers.
class InstanceExtensionWorker {
    std::atomic<bool> stopping{false};
    std::thread thread;
public:
    ~InstanceExtensionWorker() { stop(); }
    void start(std::function<void()> poll) {
        if (thread.joinable()) return;
        stopping.store(false);
        thread = std::thread([this, poll = std::move(poll)] {
            while (!stopping.load(std::memory_order_acquire)) {
                poll();
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        });
    }
    bool isCurrentThread() const { return thread.joinable() && thread.get_id() == std::this_thread::get_id(); }
    bool isStopping() const { return stopping.load(std::memory_order_acquire); }
    void requestStop() { stopping.store(true, std::memory_order_release); }
    void stop() {
        requestStop();
        if (isCurrentThread()) return; // a control/lifecycle caller will join after this poll
        if (thread.joinable()) thread.join();
    }
};
}
#endif
