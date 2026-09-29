#include "ServicePerformanceHint.h"
#include <algorithm>
#include <ctime>
#include "aap/unstable/logging.h"

#if ANDROID
#include <dlfcn.h>
#include <unistd.h>
#endif

#define LOG_TAG "AAP.PerformanceHint"

namespace {

int64_t monotonicNanos() {
    timespec ts{};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<int64_t>(ts.tv_sec) * 1000000000 + ts.tv_nsec;
}

#if ANDROID
// Resolved at runtime so that it works regardless of minSdk; notifyWorkload*() (API 36) are not in the NDK headers yet.
struct PerformanceHintApi {
    void* (*getManager)(){nullptr};
    void* (*createSession)(void* manager, const int32_t* threadIds, size_t size, int64_t initialTargetNanos){nullptr};
    int (*updateTargetWorkDuration)(void* session, int64_t targetNanos){nullptr};
    int (*reportActualWorkDuration)(void* session, int64_t actualNanos){nullptr};
    void (*closeSession)(void* session){nullptr};
    int (*setThreads)(void* session, const pid_t* threadIds, size_t size){nullptr};
    int (*notifyWorkloadIncrease)(void* session, bool cpu, bool gpu, const char* debugName){nullptr};
    int (*notifyWorkloadReset)(void* session, bool cpu, bool gpu, const char* debugName){nullptr};
    int (*notifyWorkloadSpike)(void* session, bool cpu, bool gpu, const char* debugName){nullptr};
    void* manager{nullptr};

    PerformanceHintApi() {
        auto lib = dlopen("libandroid.so", RTLD_NOW | RTLD_NODELETE);
        if (!lib)
            return;
        getManager = (decltype(getManager)) dlsym(lib, "APerformanceHint_getManager");
        createSession = (decltype(createSession)) dlsym(lib, "APerformanceHint_createSession");
        updateTargetWorkDuration = (decltype(updateTargetWorkDuration)) dlsym(lib, "APerformanceHint_updateTargetWorkDuration");
        reportActualWorkDuration = (decltype(reportActualWorkDuration)) dlsym(lib, "APerformanceHint_reportActualWorkDuration");
        closeSession = (decltype(closeSession)) dlsym(lib, "APerformanceHint_closeSession");
        setThreads = (decltype(setThreads)) dlsym(lib, "APerformanceHint_setThreads");
        notifyWorkloadIncrease = (decltype(notifyWorkloadIncrease)) dlsym(lib, "APerformanceHint_notifyWorkloadIncrease");
        notifyWorkloadReset = (decltype(notifyWorkloadReset)) dlsym(lib, "APerformanceHint_notifyWorkloadReset");
        notifyWorkloadSpike = (decltype(notifyWorkloadSpike)) dlsym(lib, "APerformanceHint_notifyWorkloadSpike");
        if (getManager && createSession && updateTargetWorkDuration && reportActualWorkDuration && closeSession)
            manager = getManager();
    }

    bool available() const { return manager != nullptr; }

    static PerformanceHintApi& get() {
        static PerformanceHintApi api{};
        return api;
    }
};
#endif

}

aap::ServicePerformanceHint::~ServicePerformanceHint() {
    closeSession();
}

void aap::ServicePerformanceHint::closeSession() {
#if ANDROID
    if (session)
        PerformanceHintApi::get().closeSession(session);
#endif
    session = nullptr;
    session_thread_count = 0;
    session_has_placeholder_thread = false;
}

int32_t aap::ServicePerformanceHint::configure(bool enabled, int64_t targetDurationNanos) {
    if (!enabled) {
        closeSession();
        return AAP_PERFORMANCE_HINT_STATUS_DISABLED;
    }
#if ANDROID
    auto& api = PerformanceHintApi::get();
    if (!api.available() || targetDurationNanos <= 0)
        return AAP_PERFORMANCE_HINT_STATUS_UNAVAILABLE;

    requested_target_nanos.store(targetDurationNanos, std::memory_order_relaxed);
    if (session) {
        if (api.updateTargetWorkDuration(session, targetDurationNanos) == 0)
            session_target_nanos = targetDurationNanos;
        return AAP_PERFORMANCE_HINT_STATUS_ACTIVE;
    }

    // The session needs at least one thread; process() will register the actual processing threads.
    int32_t initialThread = gettid();
    session = api.createSession(api.manager, &initialThread, 1, targetDurationNanos);
    if (!session) {
        aap::a_log_f(AAP_LOG_LEVEL_WARN, LOG_TAG, "Failed to create performance hint session.");
        return AAP_PERFORMANCE_HINT_STATUS_UNAVAILABLE;
    }
    session_threads[0] = initialThread;
    session_thread_count = 1;
    session_has_placeholder_thread = true;
    session_target_nanos = targetDurationNanos;
    last_duration_nanos.store(0, std::memory_order_relaxed);
    max_duration_nanos.store(0, std::memory_order_relaxed);
    return AAP_PERFORMANCE_HINT_STATUS_ACTIVE;
#else
    (void) targetDurationNanos;
    return AAP_PERFORMANCE_HINT_STATUS_UNAVAILABLE;
#endif
}

void aap::ServicePerformanceHint::applyPendingRequests() {
#if ANDROID
    auto& api = PerformanceHintApi::get();

    // Binder may dispatch process() to any thread in its pool, so collect every thread that shows up.
    // It stops calling setThreads() once all pool threads are known.
    pid_t tid = gettid();
    if (api.setThreads && std::find(session_threads, session_threads + session_thread_count, tid) == session_threads + session_thread_count) {
        if (session_has_placeholder_thread) {
            // the initial thread was the configure() caller, which is not necessarily a processing thread.
            session_has_placeholder_thread = false;
            session_threads[0] = tid;
            api.setThreads(session, session_threads, 1);
        } else if (session_thread_count < MAX_THREADS) {
            session_threads[session_thread_count++] = tid;
            api.setThreads(session, session_threads, session_thread_count);
        }
    }

    auto target = requested_target_nanos.load(std::memory_order_relaxed);
    if (target > 0 && target != session_target_nanos && api.updateTargetWorkDuration(session, target) == 0)
        session_target_nanos = target;

    switch (pending_workload.exchange(0, std::memory_order_relaxed)) {
        case AAP_PERFORMANCE_HINT_WORKLOAD_INCREASE:
            if (api.notifyWorkloadIncrease)
                api.notifyWorkloadIncrease(session, true, false, "aap-workload-increase");
            break;
        case AAP_PERFORMANCE_HINT_WORKLOAD_SPIKE:
            if (api.notifyWorkloadSpike)
                api.notifyWorkloadSpike(session, true, false, "aap-workload-spike");
            break;
        case AAP_PERFORMANCE_HINT_WORKLOAD_RESET:
            if (api.notifyWorkloadReset)
                api.notifyWorkloadReset(session, true, false, "aap-workload-reset");
            break;
        default:
            break;
    }
#endif
}

int64_t aap::ServicePerformanceHint::beginProcess() {
    if (!session)
        return 0;
    applyPendingRequests();
    return monotonicNanos();
}

void aap::ServicePerformanceHint::endProcess(int64_t beginNanos) {
    if (!session || beginNanos == 0)
        return;
    auto duration = monotonicNanos() - beginNanos;
#if ANDROID
    PerformanceHintApi::get().reportActualWorkDuration(session, duration);
#endif
    last_duration_nanos.store(duration, std::memory_order_relaxed);
    auto max = max_duration_nanos.load(std::memory_order_relaxed);
    if (duration > max)
        max_duration_nanos.store(duration, std::memory_order_relaxed);
}
