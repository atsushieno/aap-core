#include <cstring>
#include "aap/core/aapxs/latency-aapxs.h"

namespace {
int32_t getLatencyFromPlugin(AndroidAudioPlugin* plugin) {
    auto ext = (aap_latency_extension_t*) plugin->get_extension(plugin, AAP_LATENCY_EXTENSION_URI);
    return ext && ext->latency_in_samples ? ext->latency_in_samples(ext, plugin) : 0;
}

int32_t deserializeLatency(AAPXSSerializationContext* ctx) {
    int32_t latency = 0;
    if (ctx && ctx->data && ctx->data_size >= sizeof(latency) && ctx->data_capacity >= sizeof(latency))
        memcpy(&latency, ctx->data, sizeof(latency));
    return latency;
}

void notify_latency_changed(aap_latency_host_extension_t* ext, AndroidAudioPluginHost* host) {
    (void) ext;
    (void) host;
}

aap_latency_host_extension_t latency_host_receiver{nullptr, notify_latency_changed};
}

uint32_t aap::xs::AAPXSDefinition_Latency::aapxs_latency_request_flags(AAPXSDefinition* definition, bool host, int32_t opcode) {
    // No snapshot to invalidate, so the notification only coalesces.
    if (host) return opcode == OPCODE_NOTIFY_LATENCY_CHANGED ? AAPXS_REQUEST_COALESCE : 0u;
    // This declaration describes this implementation; a replaced handler owns
    // its own policy, even when it reuses this definition's URI.
    if (definition->process_incoming_plugin_aapxs_request != aapxs_latency_process_incoming_plugin_aapxs_request) return 0;
    // The plugin computes it, so it is not concurrent with processing; it does not mutate state.
    return opcode == OPCODE_GET_LATENCY ? AAPXS_REQUEST_READ_ONLY : 0u;
}

void aap::xs::AAPXSDefinition_Latency::aapxs_latency_process_incoming_plugin_aapxs_request(
        struct AAPXSDefinition *feature, AAPXSRecipientInstance *aapxsInstance,
        AndroidAudioPlugin *plugin, AAPXSRequestContext *request) {
    (void) feature;
    auto* data = request->serialization;
    switch (request->opcode) {
        case OPCODE_GET_LATENCY: {
            if (data && data->data && data->data_capacity >= sizeof(int32_t)) {
                int32_t latency = getLatencyFromPlugin(plugin);
                memcpy(data->data, &latency, sizeof(latency));
                data->data_size = sizeof(latency);
            }
            break;
        }
        default:
            if (data)
                data->data_size = 0;
            break;
    }
    aapxsInstance->send_aapxs_reply(aapxsInstance, request);
}

void aap::xs::AAPXSDefinition_Latency::aapxs_latency_process_incoming_host_aapxs_request(
        struct AAPXSDefinition *feature, AAPXSRecipientInstance *aapxsInstance,
        AndroidAudioPluginHost *host, AAPXSRequestContext *request) {
    (void) feature;
    switch (request->opcode) {
        case OPCODE_NOTIFY_LATENCY_CHANGED: {
            auto ext = (aap_latency_host_extension_t*) host->get_extension(host, AAP_LATENCY_EXTENSION_URI);
            if (ext && ext->notify_latency_changed)
                ext->notify_latency_changed(ext, host);
            break;
        }
        default:
            break;
    }
    if (request->serialization)
        request->serialization->data_size = 0;
    aapxsInstance->send_aapxs_reply(aapxsInstance, request);
}

void aap::xs::AAPXSDefinition_Latency::aapxs_latency_process_incoming_plugin_aapxs_reply(
        struct AAPXSDefinition *feature, AAPXSInitiatorInstance *aapxsInstance,
        AndroidAudioPlugin *plugin, AAPXSRequestContext *request) {
    (void) feature;
    (void) aapxsInstance;
    if (request->callback != nullptr)
        request->callback(request->callback_user_data, plugin);
}

void aap::xs::AAPXSDefinition_Latency::aapxs_latency_process_incoming_host_aapxs_reply(
        struct AAPXSDefinition *feature, AAPXSInitiatorInstance *aapxsInstance,
        AndroidAudioPluginHost *host, AAPXSRequestContext *request) {
    (void) feature;
    (void) aapxsInstance;
    if (request->callback != nullptr)
        request->callback(request->callback_user_data, host);
}

AAPXSExtensionClientProxy
aap::xs::AAPXSDefinition_Latency::aapxs_latency_get_plugin_proxy(
        struct AAPXSDefinition *feature, AAPXSInitiatorInstance *aapxsInstance,
        AAPXSSerializationContext *serialization) {
    (void) feature;
    (void) serialization;
    return AAPXSExtensionClientProxy{aapxsInstance->aapxs_context, aapxs_latency_as_plugin_extension};
}

AAPXSExtensionServiceProxy
aap::xs::AAPXSDefinition_Latency::aapxs_latency_get_host_proxy(
        struct AAPXSDefinition *feature, AAPXSInitiatorInstance *aapxsInstance,
        AAPXSSerializationContext *serialization) {
    (void) feature;
    (void) serialization;
    return AAPXSExtensionServiceProxy{aapxsInstance->aapxs_context, aapxs_latency_as_host_extension};
}

AAPXSExtensionHostReceiver
aap::xs::AAPXSDefinition_Latency::aapxs_latency_get_host_receiver(
        struct AAPXSDefinition *feature, AAPXSRecipientInstance *aapxsInstance,
        AndroidAudioPluginHost *host) {
    (void) feature;
    (void) aapxsInstance;
    (void) host;
    return AAPXSExtensionHostReceiver{nullptr, aapxs_latency_as_host_receiver};
}

void* aap::xs::AAPXSDefinition_Latency::aapxs_latency_as_host_receiver(AAPXSExtensionHostReceiver *receiver) {
    (void) receiver;
    return &latency_host_receiver;
}

// Strongly-typed client implementation (plugin extension functions)

aap::Result<int32_t> aap::xs::LatencyClientAAPXS::getLatency() {
    return callAndWait<int32_t>(OPCODE_GET_LATENCY, nullptr, 0, deserializeLatency, sizeof(int32_t));
}

int32_t aap::xs::LatencyClientAAPXS::getLatencyAsync(std::function<void(Result<int32_t>)> callback) {
    return callFunctionAsync(OPCODE_GET_LATENCY, nullptr, 0,
                             [callback = std::move(callback)](const std::string& error, AAPXSSerializationContext* ctx, void*) {
        if (!callback)
            return;
        if (!error.empty())
            callback(Result<int32_t>{0, error});
        else
            callback(Result<int32_t>{deserializeLatency(ctx), ""});
    }, sizeof(int32_t));
}

// Strongly-typed service implementation (host extension functions)

void aap::xs::LatencyServiceAAPXS::notifyLatencyChanged() {
    fireVoidFunctionAndForget(OPCODE_NOTIFY_LATENCY_CHANGED);
}
