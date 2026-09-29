#include <cstring>
#include "aap/core/aapxs/performance-hint-aapxs.h"
#include "aap/core/host/plugin-instance.h"
#include "../hosting/ServicePerformanceHint.h"

void aap::xs::AAPXSDefinition_PerformanceHint::aapxs_performance_hint_process_incoming_plugin_aapxs_request(
        struct AAPXSDefinition *feature, AAPXSRecipientInstance *aapxsInstance,
        AndroidAudioPlugin *plugin, AAPXSRequestContext *request) {
    // It is consumed by the service framework, not by the plugin.
    auto hint = ((aap::LocalPluginInstance*) aapxsInstance->host_context)->getPerformanceHint();
    auto data = request->serialization->data;
    switch (request->opcode) {
        case OPCODE_PERFORMANCE_HINT_CONFIGURE: {
            // RT_UNSAFE: always delivered via Binder, and the host reads the status from the shared buffer.
            auto config = (aap_performance_hint_configuration_t*) data;
            config->status = hint->configure(config->enabled != 0, config->target_duration_nanos);
            request->serialization->data_size = sizeof(aap_performance_hint_configuration_t);
            break;
        }
        case OPCODE_PERFORMANCE_HINT_SET_TARGET: {
            int64_t target;
            memcpy(&target, data, sizeof(int64_t));
            hint->requestTarget(target);
            break;
        }
        case OPCODE_PERFORMANCE_HINT_NOTIFY_WORKLOAD: {
            int32_t workload;
            memcpy(&workload, data, sizeof(int32_t));
            hint->requestWorkload(workload);
            break;
        }
        case OPCODE_PERFORMANCE_HINT_QUERY_TIMING: {
            auto timing = hint->takeTiming();
            memcpy(data, &timing, sizeof(timing));
            request->serialization->data_size = sizeof(timing);
            aapxsInstance->send_aapxs_reply(aapxsInstance, request);
            break;
        }
        default:
            break;
    }
}

void aap::xs::AAPXSDefinition_PerformanceHint::aapxs_performance_hint_process_incoming_host_aapxs_request(
        struct AAPXSDefinition *feature, AAPXSRecipientInstance *aapxsInstance,
        AndroidAudioPluginHost *host, AAPXSRequestContext *request) {
    // there is no host extension.
}

void aap::xs::AAPXSDefinition_PerformanceHint::aapxs_performance_hint_process_incoming_plugin_aapxs_reply(
        struct AAPXSDefinition *feature, AAPXSInitiatorInstance *aapxsInstance,
        AndroidAudioPlugin *plugin, AAPXSRequestContext *request) {
    if (request->opcode == OPCODE_PERFORMANCE_HINT_QUERY_TIMING &&
        request->serialization->data_size >= (int32_t) sizeof(aap_performance_hint_timing_t)) {
        auto client = ((aap::PluginInstance*) aapxsInstance->host_context)->getStandardExtensions().getPerformanceHintClient();
        if (client) {
            aap_performance_hint_timing_t timing;
            memcpy(&timing, request->serialization->data, sizeof(timing));
            client->storeTiming(timing);
        }
    }
    if (request->callback != nullptr)
        request->callback(request->callback_user_data, plugin);
}

void aap::xs::AAPXSDefinition_PerformanceHint::aapxs_performance_hint_process_incoming_host_aapxs_reply(
        struct AAPXSDefinition *feature, AAPXSInitiatorInstance *aapxsInstance,
        AndroidAudioPluginHost *host, AAPXSRequestContext *request) {
    // there is no host extension.
}

int32_t aap::xs::PerformanceHintClientAAPXS::configure(bool enabled, int64_t targetDurationNanos) {
    // A service that does not know this extension ignores the request and leaves the status as is.
    aap_performance_hint_configuration_t config{enabled ? 1 : 0, AAP_PERFORMANCE_HINT_STATUS_UNSUPPORTED, targetDurationNanos};
    memcpy(serialization->data, &config, sizeof(config));
    serialization->data_size = sizeof(config);
    callVoidFunctionSynchronously(OPCODE_PERFORMANCE_HINT_CONFIGURE);
    memcpy(&config, serialization->data, sizeof(config));
    return config.status;
}

void aap::xs::PerformanceHintClientAAPXS::setTarget(int64_t targetDurationNanos) {
    memcpy(serialization->data, &targetDurationNanos, sizeof(int64_t));
    serialization->data_size = sizeof(int64_t);
    fireVoidFunctionAndForget(OPCODE_PERFORMANCE_HINT_SET_TARGET);
}

void aap::xs::PerformanceHintClientAAPXS::notifyWorkload(int32_t workload) {
    memcpy(serialization->data, &workload, sizeof(int32_t));
    serialization->data_size = sizeof(int32_t);
    fireVoidFunctionAndForget(OPCODE_PERFORMANCE_HINT_NOTIFY_WORKLOAD);
}

void aap::xs::PerformanceHintClientAAPXS::queryTiming() {
    serialization->data_size = 0;
    fireVoidFunctionAndForget(OPCODE_PERFORMANCE_HINT_QUERY_TIMING);
}
