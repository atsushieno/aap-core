#include <atomic>
#include <cmath>
#include <cstring>
#include "aap/core/aapxs/tail-aapxs.h"

namespace {
// NaN until the first refresh after setup; the handler then asks the plugin under exclusion.
struct TailLengthSnapshot {
    std::atomic<double> value{std::nan("")};
};

double getTailLengthFromPlugin(AndroidAudioPlugin* plugin) {
    auto ext = (aap_tail_extension_t*) plugin->get_extension(plugin, AAP_TAIL_EXTENSION_URI);
    return ext && ext->tail_length_in_seconds ? ext->tail_length_in_seconds(ext, plugin) : 0.0;
}

double deserializeTailLength(AAPXSSerializationContext* ctx) {
    double length = 0.0;
    if (ctx && ctx->data && ctx->data_size >= sizeof(length) && ctx->data_capacity >= sizeof(length))
        memcpy(&length, ctx->data, sizeof(length));
    return length;
}

void notify_tail_length_changed(aap_tail_host_extension_t* ext, AndroidAudioPluginHost* host) {
    (void) ext;
    (void) host;
}

aap_tail_host_extension_t tail_host_receiver{nullptr, notify_tail_length_changed};
}

uint32_t aap::xs::AAPXSDefinition_Tail::aapxs_tail_request_flags(AAPXSDefinition* definition, bool host, int32_t opcode) {
    if (host) return opcode == OPCODE_NOTIFY_TAIL_LENGTH_CHANGED ? AAPXS_REQUEST_STATE_CHANGED | AAPXS_REQUEST_COALESCE : 0u;
    // This declaration describes this implementation; a replaced handler owns
    // its own policy, even when it reuses this definition's URI.
    if (definition->process_incoming_plugin_aapxs_request != aapxs_tail_process_incoming_plugin_aapxs_request) return 0;
    return opcode == OPCODE_GET_TAIL_LENGTH ? AAPXS_REQUEST_READ_ONLY | AAPXS_REQUEST_CONCURRENT : 0u;
}

bool aap::xs::AAPXSDefinition_Tail::aapxs_tail_initialize_recipient(AAPXSDefinition*, AAPXSRecipientInstance* instance, bool host) {
    if (!host) instance->aapxs_context = new TailLengthSnapshot();
    return true;
}

void aap::xs::AAPXSDefinition_Tail::aapxs_tail_release_recipient(AAPXSDefinition* definition, AAPXSRecipientInstance* instance, bool host) {
    if (!host) aapxs_tail_release_plugin_context(definition, instance->aapxs_context);
}

void aap::xs::AAPXSDefinition_Tail::aapxs_tail_state_changed(AAPXSDefinition*, AAPXSRecipientInstance* instance, AndroidAudioPlugin* plugin) {
    auto length = getTailLengthFromPlugin(plugin);
    if (!instance->aapxs_context) instance->aapxs_context = new TailLengthSnapshot();
    static_cast<TailLengthSnapshot*>(instance->aapxs_context)->value.store(length, std::memory_order_release);
}

void aap::xs::AAPXSDefinition_Tail::aapxs_tail_release_plugin_context(AAPXSDefinition*, void* context) {
    delete static_cast<TailLengthSnapshot*>(context);
}

void aap::xs::AAPXSDefinition_Tail::aapxs_tail_process_incoming_plugin_aapxs_request(
        struct AAPXSDefinition *feature, AAPXSRecipientInstance *aapxsInstance,
        AndroidAudioPlugin *plugin, AAPXSRequestContext *request) {
    (void) feature;
    auto* data = request->serialization;
    switch (request->opcode) {
        case OPCODE_GET_TAIL_LENGTH: {
            if (data && data->data && data->data_capacity >= sizeof(double)) {
                auto* snapshot = static_cast<TailLengthSnapshot*>(aapxsInstance->aapxs_context);
                double length = snapshot ? snapshot->value.load(std::memory_order_acquire) : std::nan("");
                // Before the first refresh the request is not concurrent, so the plugin can be asked.
                if (std::isnan(length))
                    length = getTailLengthFromPlugin(plugin);
                memcpy(data->data, &length, sizeof(length));
                data->data_size = sizeof(length);
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

void aap::xs::AAPXSDefinition_Tail::aapxs_tail_process_incoming_host_aapxs_request(
        struct AAPXSDefinition *feature, AAPXSRecipientInstance *aapxsInstance,
        AndroidAudioPluginHost *host, AAPXSRequestContext *request) {
    (void) feature;
    switch (request->opcode) {
        case OPCODE_NOTIFY_TAIL_LENGTH_CHANGED: {
            auto ext = (aap_tail_host_extension_t*) host->get_extension(host, AAP_TAIL_EXTENSION_URI);
            if (ext && ext->notify_tail_length_changed)
                ext->notify_tail_length_changed(ext, host);
            break;
        }
        default:
            break;
    }
    if (request->serialization)
        request->serialization->data_size = 0;
    aapxsInstance->send_aapxs_reply(aapxsInstance, request);
}

void aap::xs::AAPXSDefinition_Tail::aapxs_tail_process_incoming_plugin_aapxs_reply(
        struct AAPXSDefinition *feature, AAPXSInitiatorInstance *aapxsInstance,
        AndroidAudioPlugin *plugin, AAPXSRequestContext *request) {
    (void) feature;
    (void) aapxsInstance;
    if (request->callback != nullptr)
        request->callback(request->callback_user_data, plugin);
}

void aap::xs::AAPXSDefinition_Tail::aapxs_tail_process_incoming_host_aapxs_reply(
        struct AAPXSDefinition *feature, AAPXSInitiatorInstance *aapxsInstance,
        AndroidAudioPluginHost *host, AAPXSRequestContext *request) {
    (void) feature;
    (void) aapxsInstance;
    if (request->callback != nullptr)
        request->callback(request->callback_user_data, host);
}

AAPXSExtensionClientProxy
aap::xs::AAPXSDefinition_Tail::aapxs_tail_get_plugin_proxy(
        struct AAPXSDefinition *feature, AAPXSInitiatorInstance *aapxsInstance,
        AAPXSSerializationContext *serialization) {
    (void) feature;
    (void) serialization;
    return AAPXSExtensionClientProxy{aapxsInstance->aapxs_context, aapxs_tail_as_plugin_extension};
}

AAPXSExtensionServiceProxy
aap::xs::AAPXSDefinition_Tail::aapxs_tail_get_host_proxy(
        struct AAPXSDefinition *feature, AAPXSInitiatorInstance *aapxsInstance,
        AAPXSSerializationContext *serialization) {
    (void) feature;
    (void) serialization;
    return AAPXSExtensionServiceProxy{aapxsInstance->aapxs_context, aapxs_tail_as_host_extension};
}

AAPXSExtensionHostReceiver
aap::xs::AAPXSDefinition_Tail::aapxs_tail_get_host_receiver(
        struct AAPXSDefinition *feature, AAPXSRecipientInstance *aapxsInstance,
        AndroidAudioPluginHost *host) {
    (void) feature;
    (void) aapxsInstance;
    (void) host;
    return AAPXSExtensionHostReceiver{nullptr, aapxs_tail_as_host_receiver};
}

void* aap::xs::AAPXSDefinition_Tail::aapxs_tail_as_host_receiver(AAPXSExtensionHostReceiver *receiver) {
    (void) receiver;
    return &tail_host_receiver;
}

// Strongly-typed client implementation (plugin extension functions)

aap::Result<double> aap::xs::TailClientAAPXS::getTailLength() {
    return callAndWait<double>(OPCODE_GET_TAIL_LENGTH, nullptr, 0, deserializeTailLength, sizeof(double));
}

int32_t aap::xs::TailClientAAPXS::getTailLengthAsync(std::function<void(Result<double>)> callback) {
    return callFunctionAsync(OPCODE_GET_TAIL_LENGTH, nullptr, 0,
                             [callback = std::move(callback)](const std::string& error, AAPXSSerializationContext* ctx, void*) {
        if (!callback)
            return;
        if (!error.empty())
            callback(Result<double>{0.0, error});
        else
            callback(Result<double>{deserializeTailLength(ctx), ""});
    }, sizeof(double));
}

// Strongly-typed service implementation (host extension functions)

void aap::xs::TailServiceAAPXS::notifyTailLengthChanged() {
    fireVoidFunctionAndForget(OPCODE_NOTIFY_TAIL_LENGTH_CHANGED);
}
