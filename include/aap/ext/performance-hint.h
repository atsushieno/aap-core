#ifndef AAP_PERFORMANCE_HINT_H_INCLUDED
#define AAP_PERFORMANCE_HINT_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

#include "../android-audio-plugin.h"
#include "stdint.h"

#define AAP_PERFORMANCE_HINT_EXTENSION_URI "urn://androidaudioplugin.org/extensions/performance-hint/v1"

/*
 * Performance hint extension lets a host drive an ADPF (Android Dynamic Performance Framework)
 * hint session that the plugin service creates for the threads that run `process()`.
 *
 * The host cannot put the service's threads into its own hint session (they belong to another
 * process), so the service owns the session and the host supplies the policy: whether it is
 * enabled, the time budget for each `process()` call, and workload change notifications.
 *
 * This extension is consumed by the AAP service framework (libandroidaudioplugin), not by plugins.
 * Plugins do not implement it, and `get_extension()` on a plugin never returns it.
 *
 * Opcodes and routes:
 * - configure: RT_UNSAFE. Send it while the instance is not ACTIVE. The service creates, retargets,
 *   or closes its hint session, and writes the resulting status into the shared payload.
 *   A service that does not know this extension leaves the status untouched, so hosts should
 *   pre-fill `status` with AAP_PERFORMANCE_HINT_STATUS_UNSUPPORTED to detect it.
 * - set_target: RT_SAFE. Updates the target duration of the session.
 * - notify_workload: RT_SAFE. Forwards a workload change notification to the session.
 * - query_timing: RT_SAFE. Replies the duration of the latest `process()` and the maximum
 *   duration since the previous query.
 *
 * Hosts must not send RT_SAFE requests unless configure returned AAP_PERFORMANCE_HINT_STATUS_ACTIVE.
 */

enum aap_performance_hint_status_t {
    // The service does not support this extension.
    AAP_PERFORMANCE_HINT_STATUS_UNSUPPORTED = 0,
    // The service created (or retained) its hint session.
    AAP_PERFORMANCE_HINT_STATUS_ACTIVE = 1,
    // The service supports this extension but the device does not provide ADPF sessions.
    AAP_PERFORMANCE_HINT_STATUS_UNAVAILABLE = 2,
    // The service closed its hint session as requested.
    AAP_PERFORMANCE_HINT_STATUS_DISABLED = 3,
};

enum aap_performance_hint_workload_t {
    AAP_PERFORMANCE_HINT_WORKLOAD_INCREASE = 1,
    AAP_PERFORMANCE_HINT_WORKLOAD_SPIKE = 2,
    AAP_PERFORMANCE_HINT_WORKLOAD_RESET = 3,
};

typedef struct aap_performance_hint_configuration_t {
    int32_t enabled;
    int32_t status;
    int64_t target_duration_nanos;
} aap_performance_hint_configuration_t;

typedef struct aap_performance_hint_timing_t {
    int64_t last_duration_nanos;
    int64_t max_duration_nanos;
} aap_performance_hint_timing_t;

#ifdef __cplusplus
} // extern "C"
#endif

#endif // AAP_PERFORMANCE_HINT_H_INCLUDED
