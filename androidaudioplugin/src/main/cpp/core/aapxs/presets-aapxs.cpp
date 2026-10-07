#include <atomic>
#include "aap/core/aapxs/presets-aapxs.h"
#include "aap/core/host/plugin-instance.h"

namespace {
struct PresetsCountSnapshot {
    std::atomic<int32_t> value;
    explicit PresetsCountSnapshot(int32_t count) : value(count) {}
};
}

namespace {
void notify_preset_loaded(aap_presets_host_extension_t* ext, AndroidAudioPluginHost* host) {
    (void) ext;
    (void) host;
}

void notify_presets_updated(aap_presets_host_extension_t* ext, AndroidAudioPluginHost* host) {
    (void) ext;
    (void) host;
}

aap_presets_host_extension_t presets_host_receiver{nullptr, notify_preset_loaded, notify_presets_updated};
}

uint32_t aap::xs::AAPXSDefinition_Presets::aapxs_presets_request_flags(AAPXSDefinition* definition, bool host, int32_t opcode) {
    if (host) return (opcode == OPCODE_NOTIFY_PRESET_LOADED || opcode == OPCODE_NOTIFY_PRESETS_UPDATED) ? AAPXS_REQUEST_STATE_CHANGED | AAPXS_REQUEST_COALESCE : 0u;
    // This declaration describes this implementation; a replaced handler owns
    // its own policy, even when it reuses this definition's URI.
    if (definition->process_incoming_plugin_aapxs_request != aapxs_presets_process_incoming_plugin_aapxs_request) return 0;
    switch (opcode) {
        case OPCODE_GET_PRESET_COUNT: return AAPXS_REQUEST_READ_ONLY | AAPXS_REQUEST_CONCURRENT;
        case OPCODE_GET_PRESET_DATA:
            return AAPXS_REQUEST_READ_ONLY;
        default: return 0;
    }
}
bool aap::xs::AAPXSDefinition_Presets::aapxs_presets_initialize_recipient(AAPXSDefinition*, AAPXSRecipientInstance* instance, bool host) {
    if (!host) instance->aapxs_context = new PresetsCountSnapshot(-1);
    return true;
}
void aap::xs::AAPXSDefinition_Presets::aapxs_presets_release_recipient(AAPXSDefinition* definition, AAPXSRecipientInstance* instance, bool host) {
    if (!host) aapxs_presets_release_plugin_context(definition, instance->aapxs_context);
}
void aap::xs::AAPXSDefinition_Presets::aapxs_presets_state_changed(AAPXSDefinition*, AAPXSRecipientInstance* instance, AndroidAudioPlugin* plugin) {
    auto* extension = static_cast<aap_presets_extension_t*>(plugin->get_extension(plugin, AAP_PRESETS_EXTENSION_URI));
    auto count = extension && extension->get_preset_count ? extension->get_preset_count(extension, plugin) : 0;
    if (!instance->aapxs_context) instance->aapxs_context = new PresetsCountSnapshot(count);
    else static_cast<PresetsCountSnapshot*>(instance->aapxs_context)->value.store(count, std::memory_order_release);
}
void aap::xs::AAPXSDefinition_Presets::aapxs_presets_release_plugin_context(AAPXSDefinition*, void* context) {
    delete static_cast<PresetsCountSnapshot*>(context);
}

void aap::xs::AAPXSDefinition_Presets::aapxs_presets_process_incoming_plugin_aapxs_request(
        struct AAPXSDefinition *feature, AAPXSRecipientInstance *aapxsInstance,
        AndroidAudioPlugin *plugin, AAPXSRequestContext *request) {
    if (request->opcode == OPCODE_GET_PRESET_COUNT && aapxsInstance->aapxs_context) {
        const auto count = static_cast<PresetsCountSnapshot*>(aapxsInstance->aapxs_context)->value.load(std::memory_order_acquire);
        memcpy(request->serialization->data, &count, sizeof(count));
        request->serialization->data_size = sizeof(count);
        aapxsInstance->send_aapxs_reply(aapxsInstance, request);
        return;
    }
    auto ext = (aap_presets_extension_t*) plugin->get_extension(plugin, AAP_PRESETS_EXTENSION_URI);
    switch(request->opcode) {
        case OPCODE_GET_PRESET_COUNT:
            *((int32_t*) request->serialization->data) = ext ? ext->get_preset_count(ext, plugin) : 0;
            request->serialization->data_size = sizeof(int32_t);
            // RT_SAFE. Send reply now.
            aapxsInstance->send_aapxs_reply(aapxsInstance, request);
            break;
        case OPCODE_SET_PRESET_INDEX: {
            if (ext) {
                int32_t index = *((int32_t *) request->serialization->data);

                // FIXME: should this be calling it asynchronously?
                //  Async invoker foundation should be backed by AAPXSRecipientInstance though.

                ext->set_preset_index(ext, plugin, index);
            }

            request->serialization->data_size = 0;
            aapxsInstance->send_aapxs_reply(aapxsInstance, request);
            break;
        }
        case OPCODE_GET_PRESET_DATA: {
            if (ext) {
                // request (offset-range: content)
                // - 0..3 : int32_t index
                // - 4..7 : bool skip binary or not
                aap_preset_t preset{};
                int32_t index = *((int32_t *) request->serialization->data);

                // FIXME: should this be calling it asynchronously?
                //  Async invoker foundation should be backed by AAPXSRecipientInstance though.

                ext->get_preset(ext, plugin, index, &preset, nullptr,
                                nullptr); // we can pass null callback as plugins implement it in synchronous way
                // response (offset-range: content)
                // - 0..3 : stable ID
                // - 4..259 : name (fixed length char buffer)
                *((int32_t *) request->serialization->data) = preset.id;
                auto name = (char *) request->serialization->data + sizeof(int32_t);
                memset(name, 0, AAP_PRESETS_EXTENSION_MAX_NAME_LENGTH);
                strncpy(name, preset.name, AAP_PRESETS_EXTENSION_MAX_NAME_LENGTH - 1);
            }
            request->serialization->data_size = sizeof(int32_t) + AAP_PRESETS_EXTENSION_MAX_NAME_LENGTH;
            aapxsInstance->send_aapxs_reply(aapxsInstance, request);
            break;
        }
    }
}

void aap::xs::AAPXSDefinition_Presets::aapxs_presets_process_incoming_host_aapxs_request(
        struct AAPXSDefinition *feature, AAPXSRecipientInstance *aapxsInstance,
        AndroidAudioPluginHost *host, AAPXSRequestContext *request) {
    auto ext = (aap_presets_host_extension_t*) host->get_extension(host, AAP_PRESETS_EXTENSION_URI);
    if (!ext)
        return; // FIXME: should there be any global error handling?
    switch(request->opcode) {
        case OPCODE_NOTIFY_PRESET_LOADED:
            // no args, no return
            ext->notify_preset_loaded(ext, host);
            aapxsInstance->send_aapxs_reply(aapxsInstance, request);
            break;
        case OPCODE_NOTIFY_PRESETS_UPDATED:
            // no args, no return
            ext->notify_presets_updated(ext, host);
            aapxsInstance->send_aapxs_reply(aapxsInstance, request);
            break;
    }
}

void aap::xs::AAPXSDefinition_Presets::aapxs_presets_process_incoming_plugin_aapxs_reply(
        struct AAPXSDefinition *feature, AAPXSInitiatorInstance *aapxsInstance,
        AndroidAudioPlugin *plugin, AAPXSRequestContext *request) {
    if (request->callback != nullptr)
        request->callback(request->callback_user_data, plugin);
}

void aap::xs::AAPXSDefinition_Presets::aapxs_presets_process_incoming_host_aapxs_reply(
        struct AAPXSDefinition *feature, AAPXSInitiatorInstance *aapxsInstance,
        AndroidAudioPluginHost *host, AAPXSRequestContext *request) {
    if (request->callback != nullptr)
        request->callback(request->callback_user_data, host);
}

AAPXSExtensionClientProxy
aap::xs::AAPXSDefinition_Presets::aapxs_presets_get_plugin_proxy(struct AAPXSDefinition *feature,
                                                                 AAPXSInitiatorInstance *aapxsInstance,
                                                                 AAPXSSerializationContext *serialization) {
    (void) feature;
    (void) serialization;
    return AAPXSExtensionClientProxy{aapxsInstance->aapxs_context, aapxs_presets_as_plugin_extension};
}

AAPXSExtensionServiceProxy
aap::xs::AAPXSDefinition_Presets::aapxs_presets_get_host_proxy(struct AAPXSDefinition *feature,
                                                               AAPXSInitiatorInstance *aapxsInstance,
                                                               AAPXSSerializationContext *serialization) {
    (void) feature;
    (void) serialization;
    return AAPXSExtensionServiceProxy{aapxsInstance->aapxs_context, aapxs_presets_as_host_extension};
}

void aap::xs::AAPXSDefinition_Presets::aapxs_presets_release_instance_context(
        struct AAPXSDefinition* feature, void* aapxsContext) {
    (void) feature;
    delete static_cast<TypedAAPXS*>(aapxsContext);
}

AAPXSExtensionHostReceiver
aap::xs::AAPXSDefinition_Presets::aapxs_presets_get_host_receiver(
        struct AAPXSDefinition *feature,
        AAPXSRecipientInstance *aapxsInstance,
        AndroidAudioPluginHost *host) {
    (void) feature;
    (void) aapxsInstance;
    (void) host;
    return AAPXSExtensionHostReceiver{nullptr, aapxs_presets_as_host_receiver};
}

void* aap::xs::AAPXSDefinition_Presets::aapxs_presets_as_host_receiver(
        AAPXSExtensionHostReceiver *receiver) {
    (void) receiver;
    return &presets_host_receiver;
}

// Strongly-typed client implementation (plugin extension functions)

int32_t aap::xs::PresetsClientAAPXS::getPresetCount() {
    auto result = callAndWait<int32_t>(OPCODE_GET_PRESET_COUNT, nullptr, 0,
                                       [](AAPXSSerializationContext* ctx) -> int32_t {
        return getTypedResult<int32_t>(ctx);
    });
    return result.isOk() ? result.value : 0;
}

namespace {
    // response (offset-range: content)
    // - 0..3 : stable ID
    // - 4..259 : name (fixed length char buffer)
    aap_preset_t deserializePreset(AAPXSSerializationContext* ctx) {
        aap_preset_t preset{};
        preset.id = *((int32_t *) ctx->data);
        strncpy(preset.name, (const char *) ((uint8_t *) ctx->data + sizeof(int32_t)), AAP_PRESETS_EXTENSION_MAX_NAME_LENGTH);
        return preset;
    }
}

std::string aap::xs::PresetsClientAAPXS::getPreset(int32_t index, aap_preset_t &preset) {
    // request: 0..3 index
    return callAndWait<bool>(OPCODE_GET_PRESET_DATA, &index, sizeof(index), [&preset](AAPXSSerializationContext* ctx) -> bool {
        preset = deserializePreset(ctx);
        return true;
    }).error;
}

std::string aap::xs::PresetsClientAAPXS::setPresetIndex(int32_t index) {
    return callAndWait<bool>(OPCODE_SET_PRESET_INDEX, &index, sizeof(index), [](AAPXSSerializationContext*) -> bool { return true; }).error;
}

int32_t aap::xs::PresetsClientAAPXS::getPresetAsync(int32_t index, std::function<void(Result<aap_preset_t>)> callback) {
    if (aap::RealtimeScope::isActive()) return -1;
    return callFunctionAsync(OPCODE_GET_PRESET_DATA, &index, sizeof(index),
                             [callback = std::move(callback)](const std::string& error, AAPXSSerializationContext* ctx, void*) {
        if (!callback)
            return;
        if (!error.empty())
            callback(Result<aap_preset_t>{aap_preset_t{}, error});
        else
            callback(Result<aap_preset_t>{deserializePreset(ctx), ""});
    });
}

int32_t aap::xs::PresetsClientAAPXS::setPresetIndexAsync(int32_t index, std::function<void(Result<bool>)> callback) {
    if (aap::RealtimeScope::isActive()) return -1;
    return callFunctionAsync(OPCODE_SET_PRESET_INDEX, &index, sizeof(index),
                             [callback = std::move(callback)](const std::string& error, AAPXSSerializationContext*, void*) {
        if (callback)
            callback(Result<bool>{error.empty(), error});
    });
}

// Strongly-typed service implementation (host extension functions)

void aap::xs::PresetsServiceAAPXS::notifyPresetLoaded() {
    fireVoidFunctionAndForget(OPCODE_NOTIFY_PRESET_LOADED);
}

void aap::xs::PresetsServiceAAPXS::notifyPresetsUpdated() {
    fireVoidFunctionAndForget(OPCODE_NOTIFY_PRESETS_UPDATED);
}
