#ifndef AAP_CORE_SERVICE_PERFORMANCE_HINT_H
#define AAP_CORE_SERVICE_PERFORMANCE_HINT_H

#include <atomic>
#include <cstdint>
#include <sys/types.h>
#include "aap/ext/performance-hint.h"

namespace aap {

    /**
     * Service-side ADPF hint session for the threads that run LocalPluginInstance::process().
     *
     * configure() is invoked by the performance-hint AAPXS in non-realtime context.
     * request*() and takeTiming() are invoked by the AAPXS on the process() thread.
     * beginProcess()/endProcess() wrap the plugin's process() call.
     * All session calls except creation and closing happen on the process() thread; the owner must
     * make sure configure() never runs concurrently with process() (LocalPluginInstance serializes
     * them with its plugin call lock).
     */
    class ServicePerformanceHint {
        static const int32_t MAX_THREADS = 16;

        void* session{nullptr};
        int64_t session_target_nanos{0};
        pid_t session_threads[MAX_THREADS]{};
        int32_t session_thread_count{0};
        bool session_has_placeholder_thread{false};

        std::atomic<int64_t> requested_target_nanos{0};
        std::atomic<int32_t> pending_workload{0};
        std::atomic<int64_t> last_duration_nanos{0};
        std::atomic<int64_t> max_duration_nanos{0};

        void closeSession();
        void applyPendingRequests();

    public:
        ~ServicePerformanceHint();

        // returns one of aap_performance_hint_status_t.
        int32_t configure(bool enabled, int64_t targetDurationNanos);

        void requestTarget(int64_t targetDurationNanos) {
            requested_target_nanos.store(targetDurationNanos, std::memory_order_relaxed);
        }
        void requestWorkload(int32_t workload) {
            pending_workload.store(workload, std::memory_order_relaxed);
        }
        aap_performance_hint_timing_t takeTiming() {
            return {last_duration_nanos.load(std::memory_order_relaxed),
                    max_duration_nanos.exchange(0, std::memory_order_relaxed)};
        }

        bool isActive() const { return session != nullptr; }
        // returns the begin timestamp to pass to endProcess(), or 0 if there is no session.
        int64_t beginProcess();
        void endProcess(int64_t beginNanos);
    };
}

#endif //AAP_CORE_SERVICE_PERFORMANCE_HINT_H
