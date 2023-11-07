
#include "aap/core/aapxs/presets-aapxs.h"

void aap::xs::AAPXSDefinition_Presets::aapxs_presets_process_incoming_plugin_aapxs_request(
        struct AAPXSDefinition *feature, AAPXSRecipientInstance *aapxsInstance,
        AndroidAudioPlugin *plugin, AAPXSRequestContext *request) {
    auto ext = (aap_presets_extension_t*) plugin->get_extension(plugin, AAP_PRESETS_EXTENSION_URI);
    switch(request->opcode) {
        case OPCODE_GET_PRESET_COUNT:
            *((int32_t*) request->serialization->data) = ext ? ext->get_preset_count(ext, plugin) : 0;
            // RT_SAFE. Send reply now.
            aapxsInstance->send_aapxs_reply(aapxsInstance, request);
            break;
        case OPCODE_GET_PRESET_INDEX:
            *((int32_t*) request->serialization->data) = ext ? ext->get_preset_index(ext, plugin) : 0;
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

            aapxsInstance->send_aapxs_reply(aapxsInstance, request);
            break;
        }
        case OPCODE_GET_PRESET_DATA: {
            if (ext) {
                // request (offset-range: content)
                // - 0..3 : int32_t index
                // - 4..7 : bool skip binary or not
                aap_preset_t preset;
                int32_t index = *((int32_t *) request->serialization->data);

                // FIXME: should this be calling it asynchronously?
                //  Async invoker foundation should be backed by AAPXSRecipientInstance though.

                ext->get_preset(ext, plugin, index, &preset, nullptr,
                                nullptr); // we can pass null callback as plugins implement it in synchronous way
                // response (offset-range: content)
                // - 0..3 : stable ID
                // - 4..259 : name (fixed length char buffer)
                *((int32_t *) request->serialization->data) = preset.id;
                size_t len = strlen(preset.name);
                strncpy((char *) request->serialization->data + sizeof(int32_t),
                        const_cast<char *const>(preset.name), len);
                ((char *) request->serialization->data)[len + sizeof(int32_t)] = 0;
            }
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
        request->callback(request->callback_user_data, plugin, request->request_id);
}

void aap::xs::AAPXSDefinition_Presets::aapxs_presets_process_incoming_host_aapxs_reply(
        struct AAPXSDefinition *feature, AAPXSInitiatorInstance *aapxsInstance,
        AndroidAudioPluginHost *host, AAPXSRequestContext *request) {
    if (request->callback != nullptr)
        request->callback(request->callback_user_data, host, request->request_id);
}

AAPXSExtensionClientProxy
aap::xs::AAPXSDefinition_Presets::aapxs_presets_get_plugin_proxy(struct AAPXSDefinition *feature,
                                                                 AAPXSInitiatorInstance *aapxsInstance,
                                                                 AAPXSSerializationContext *serialization) {
    auto client = (AAPXSDefinition_Presets*) feature->aapxs_context;
    if (!client->typed_client)
        client->typed_client = std::make_unique<PresetsClientAAPXS>(aapxsInstance, serialization);
    *client->typed_client = PresetsClientAAPXS(aapxsInstance, serialization);
    client->client_proxy = AAPXSExtensionClientProxy{client->typed_client.get(), aapxs_presets_as_plugin_extension};
    return client->client_proxy;
}

AAPXSExtensionServiceProxy
aap::xs::AAPXSDefinition_Presets::aapxs_presets_get_host_proxy(struct AAPXSDefinition *feature,
                                                               AAPXSInitiatorInstance *aapxsInstance,
                                                               AAPXSSerializationContext *serialization) {
    auto service = (AAPXSDefinition_Presets*) feature->aapxs_context;
    if (!service->typed_service)
        service->typed_service = std::make_unique<PresetsServiceAAPXS>(aapxsInstance, serialization);
    *service->typed_service = PresetsServiceAAPXS(aapxsInstance, serialization);
    service->service_proxy = AAPXSExtensionServiceProxy{&service->typed_service, aapxs_presets_as_host_extension};
    return service->service_proxy;
}

// Strongly-typed client implementation (plugin extension functions)

int32_t aap::xs::PresetsClientAAPXS::getPresetCount() {
    return callTypedFunctionSynchronously<int32_t>(OPCODE_GET_PRESET_COUNT);
}

int32_t aap::xs::PresetsClientAAPXS::getPresetIndex() {
    return callTypedFunctionSynchronously<int32_t>(OPCODE_GET_PRESET_INDEX);
}

void aap::xs::PresetsClientAAPXS::getPreset(int32_t index, aap_preset_t &preset) {
    // request
    // - 0..3: index
    *(int32_t*) (serialization->data) = index;

    callVoidFunctionSynchronously(OPCODE_GET_PRESET_DATA);

    // response
    // - 0..3 : stable ID
    // - 4..259 : name (fixed length char buffer)
    preset.id = *((int32_t *) serialization->data);
    strncpy(preset.name, (const char *) ((uint8_t *) serialization->data + sizeof(int32_t)), AAP_PRESETS_EXTENSION_MAX_NAME_LENGTH);
    // return nothing
}

void aap::xs::PresetsClientAAPXS::setPresetIndex(int32_t index) {
    *(int32_t*) (serialization->data) = index;
    callVoidFunctionSynchronously(OPCODE_SET_PRESET_INDEX);
}

// Strongly-typed service implementation (host extension functions)

void aap::xs::PresetsServiceAAPXS::notifyPresetLoaded() {
    callVoidFunctionSynchronously(OPCODE_NOTIFY_PRESET_LOADED);
}

void aap::xs::PresetsServiceAAPXS::notifyPresetsUpdated() {
    callVoidFunctionSynchronously(OPCODE_NOTIFY_PRESETS_UPDATED);
}
