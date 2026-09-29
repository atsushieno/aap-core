#ifndef AAP_CORE_PERFORMANCE_HINT_AAPXS_H
#define AAP_CORE_PERFORMANCE_HINT_AAPXS_H

#include <atomic>
#include "aap/aapxs.h"
#include "../../ext/performance-hint.h"
#include "typed-aapxs.h"

// plugin extension opcodes
const int32_t OPCODE_PERFORMANCE_HINT_CONFIGURE = 1;
const int32_t OPCODE_PERFORMANCE_HINT_SET_TARGET = 2;
const int32_t OPCODE_PERFORMANCE_HINT_NOTIFY_WORKLOAD = 3;
const int32_t OPCODE_PERFORMANCE_HINT_QUERY_TIMING = 4;

const int32_t PERFORMANCE_HINT_SHARED_MEMORY_SIZE = 32;

namespace aap::xs {
    // Host-side client. configure() is RT_UNSAFE and must be called while the instance is not ACTIVE.
    // The other requests are RT_SAFE and must only be sent while the instance is ACTIVE,
    // otherwise they fall back to synchronous Binder calls.
    class PerformanceHintClientAAPXS : public TypedAAPXS {
        std::atomic<int64_t> last_duration_nanos{0};
        std::atomic<int64_t> max_duration_nanos{0};

    public:
        PerformanceHintClientAAPXS(AAPXSInitiatorInstance* initiatorInstance, AAPXSSerializationContext* serialization)
                : TypedAAPXS(AAP_PERFORMANCE_HINT_EXTENSION_URI, initiatorInstance, serialization) {
        }

        // returns one of aap_performance_hint_status_t.
        int32_t configure(bool enabled, int64_t targetDurationNanos);
        void setTarget(int64_t targetDurationNanos);
        void notifyWorkload(int32_t workload);
        // The reply arrives within a later process() call and is reflected to getTiming().
        void queryTiming();

        void storeTiming(const aap_performance_hint_timing_t& timing) {
            last_duration_nanos.store(timing.last_duration_nanos, std::memory_order_relaxed);
            max_duration_nanos.store(timing.max_duration_nanos, std::memory_order_relaxed);
        }
        aap_performance_hint_timing_t getTiming() const {
            return {last_duration_nanos.load(std::memory_order_relaxed),
                    max_duration_nanos.load(std::memory_order_relaxed)};
        }
    };

    class AAPXSDefinition_PerformanceHint : public AAPXSDefinitionWrapper {
        static void aapxs_performance_hint_process_incoming_plugin_aapxs_request(
                struct AAPXSDefinition* feature,
                AAPXSRecipientInstance* aapxsInstance,
                AndroidAudioPlugin* plugin,
                AAPXSRequestContext* request);
        static void aapxs_performance_hint_process_incoming_host_aapxs_request(
                struct AAPXSDefinition* feature,
                AAPXSRecipientInstance* aapxsInstance,
                AndroidAudioPluginHost* host,
                AAPXSRequestContext* request);
        static void aapxs_performance_hint_process_incoming_plugin_aapxs_reply(
                struct AAPXSDefinition* feature,
                AAPXSInitiatorInstance* aapxsInstance,
                AndroidAudioPlugin* plugin,
                AAPXSRequestContext* request);
        static void aapxs_performance_hint_process_incoming_host_aapxs_reply(
                struct AAPXSDefinition* feature,
                AAPXSInitiatorInstance* aapxsInstance,
                AndroidAudioPluginHost* host,
                AAPXSRequestContext* request);

        static bool aapxs_performance_hint_is_command_rt_safe(struct AAPXSDefinition*, bool isHostExtension, int32_t opcode) {
            if (isHostExtension)
                return false;
            return opcode == OPCODE_PERFORMANCE_HINT_SET_TARGET ||
                   opcode == OPCODE_PERFORMANCE_HINT_NOTIFY_WORKLOAD ||
                   opcode == OPCODE_PERFORMANCE_HINT_QUERY_TIMING;
        }

        AAPXSDefinition aapxs_performance_hint{this,
                                               AAP_PERFORMANCE_HINT_EXTENSION_URI,
                                               PERFORMANCE_HINT_SHARED_MEMORY_SIZE,
                                               aapxs_performance_hint_process_incoming_plugin_aapxs_request,
                                               aapxs_performance_hint_process_incoming_host_aapxs_request,
                                               aapxs_performance_hint_process_incoming_plugin_aapxs_reply,
                                               aapxs_performance_hint_process_incoming_host_aapxs_reply,
                                               // consumed by the service framework, never exposed as a plugin extension.
                                               nullptr,
                                               nullptr, // no host extension
                                               aapxs_performance_hint_is_command_rt_safe,
                                               nullptr
        };

    public:
        AAPXSDefinition& asPublic() override {
            return aapxs_performance_hint;
        }
    };
}

#endif //AAP_CORE_PERFORMANCE_HINT_AAPXS_H
