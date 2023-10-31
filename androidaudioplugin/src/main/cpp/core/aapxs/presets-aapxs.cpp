
#include "presets-aapxs.h"
#include "aap/android-audio-plugin.h"
#include "aap/core/aapxs/aapxs-runtime.h"

void aap::xs::AAPXSDefinition_Presets::aapxs_presets_process_incoming_plugin_aapxs_request(
        struct AAPXSDefinition *feature, AAPXSRecipientInstance *aapxsInstance,
        AndroidAudioPlugin *plugin, AAPXSRequestContext *context) {
    auto ext = (aap_presets_extension_t*) plugin->get_extension(plugin, AAP_PRESETS_EXTENSION_URI);
    switch(context->opcode) {
        case OPCODE_GET_PRESET_COUNT:
            *((int32_t*) context->serialization->data) = ext->get_preset_count(ext, plugin);
            // RT_SAFE. Send reply now.
            aapxsInstance->send_aapxs_reply(aapxsInstance, context);
            break;
        case OPCODE_GET_PRESET_INDEX:
            *((int32_t*) context->serialization->data) = ext->get_preset_index(ext, plugin);
            // RT_SAFE. Send reply now.
            aapxsInstance->send_aapxs_reply(aapxsInstance, context);
            break;
        case OPCODE_SET_PRESET_INDEX: {
            int32_t index = *((int32_t *) context->serialization->data);

            // FIXME: should this be calling it asynchronously?
            //  Async invoker foundation should be backed by AAPXSRecipientInstance though.

            ext->set_preset_index(ext, plugin, index);

            aapxsInstance->send_aapxs_reply(aapxsInstance, context);
            break;
        }
        case OPCODE_GET_PRESET_DATA: {
            // request (offset-range: content)
            // - 0..3 : int32_t index
            // - 4..7 : bool skip binary or not
            aap_preset_t preset;
            int32_t index = *((int32_t *) context->serialization->data);

            // FIXME: should this be calling it asynchronously?
            //  Async invoker foundation should be backed by AAPXSRecipientInstance though.

            ext->get_preset(ext, plugin, index, &preset, nullptr,
                            nullptr); // we can pass null callback as plugins implement it in synchronous way
            // response (offset-range: content)
            // - 0..3 : stable ID
            // - 4..259 : name (fixed length char buffer)
            *((int32_t *) context->serialization->data) = preset.id;
            size_t len = strlen(preset.name);
            strncpy((char *) context->serialization->data + sizeof(int32_t),
                    const_cast<char *const>(preset.name), len);
            ((char *) context->serialization->data)[len + sizeof(int32_t)] = 0;

            aapxsInstance->send_aapxs_reply(aapxsInstance, context);
            break;
        }
    }
}

void aap::xs::AAPXSDefinition_Presets::aapxs_presets_process_incoming_host_aapxs_request(
        struct AAPXSDefinition *feature, AAPXSRecipientInstance *aapxsInstance,
        AndroidAudioPluginHost *host, AAPXSRequestContext *context) {
    auto ext = (aap_presets_host_extension_t*) host->get_extension(host, AAP_PRESETS_EXTENSION_URI);
    switch(context->opcode) {
        case OPCODE_NOTIFY_PRESET_LOADED:
            // no args, no return
            ext->notify_preset_loaded(ext, host);
            aapxsInstance->send_aapxs_reply(aapxsInstance, context);
            break;
        case OPCODE_NOTIFY_PRESET_UPDATED:
            // no args, no return
            ext->notify_presets_update(ext, host);
            aapxsInstance->send_aapxs_reply(aapxsInstance, context);
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
        AndroidAudioPluginHost *host, AAPXSRequestContext *context) {
    //auto ext = (aap_presets_host_extension_t*) host->get_extension(host, AAP_PRESETS_EXTENSION_URI);
    switch(context->opcode) {
        case OPCODE_NOTIFY_PRESET_LOADED:
            // nothing to handle
            break;
        case OPCODE_NOTIFY_PRESET_UPDATED:
            // nothing to handle
            break;
    }
}

// Strongly-typed client implementation (plugin extension functions)

void aap::xs::PresetsClientAAPXS::getPresetIntCallback(void* callbackContext, AndroidAudioPlugin* plugin, int32_t requestId) {
    auto callbackData = (WithPromise<PresetsClientAAPXS, int32_t>*) callbackContext;
    auto thiz = (PresetsClientAAPXS*) callbackData->context;
    int32_t result = *(int32_t*) (thiz->serialization->data);
    callbackData->promise->set_value(result);
}

int32_t aap::xs::PresetsClientAAPXS::callIntPresetFunction(int32_t opcode) {
    // FIXME: use spinlock instead of std::promise and std::future, as getPresetCount() and getPresetIndex() must be RT_SAFE.
    std::promise<int32_t> promise{};
    uint32_t requestId = initiatorInstance->get_new_request_id(initiatorInstance);
    auto future = promise.get_future();
    WithPromise<PresetsClientAAPXS, int32_t> callbackData{this, &promise};
    AAPXSRequestContext request{getPresetIntCallback, &callbackData, serialization, AAP_PRESETS_EXTENSION_URI, requestId, opcode};

    initiatorInstance->send_aapxs_request(initiatorInstance, &request);

    future.wait();
    return future.get();
}

int32_t aap::xs::PresetsClientAAPXS::getPresetCount() {
    return callIntPresetFunction(OPCODE_GET_PRESET_COUNT);
}

int32_t aap::xs::PresetsClientAAPXS::getPresetIndex() {
    return callIntPresetFunction(OPCODE_GET_PRESET_INDEX);
}

void aap::xs::PresetsClientAAPXS::getPresetCallback(void* callbackContext, AndroidAudioPlugin* plugin, int32_t requestId) {
    auto callbackData = (WithPromise<PresetsClientAAPXS, int32_t>*) callbackContext;
    auto thiz = (PresetsClientAAPXS*) callbackData->context;
    int32_t result = *(int32_t*) (thiz->serialization->data);
    callbackData->promise->set_value(result);
}

void aap::xs::PresetsClientAAPXS::getPreset(int32_t index, aap_preset_t &preset) {
    // FIXME: use spinlock instead of std::promise and std::future, as getPresetCount() and getPresetIndex() must be RT_SAFE.
    std::promise<int32_t> promise{};
    uint32_t requestId = initiatorInstance->get_new_request_id(initiatorInstance);
    auto future = promise.get_future();
    WithPromise<PresetsClientAAPXS, int32_t> callbackData{this, &promise};
    AAPXSRequestContext request{getPresetCallback, &callbackData, serialization, AAP_PRESETS_EXTENSION_URI, requestId, OPCODE_GET_PRESET_DATA};

    *(int32_t*) (serialization->data) = index;
    initiatorInstance->send_aapxs_request(initiatorInstance, &request);

    future.wait();
    // response (offset-range: content)
    // - 0..3 : stable ID
    // - 4..259 : name (fixed length char buffer)
    preset.id = *((int32_t *) serialization->data);
    strncpy(preset.name, (const char *) ((uint8_t *) serialization->data + sizeof(int32_t)), AAP_PRESETS_EXTENSION_MAX_NAME_LENGTH);
    // return nothing
}

void aap::xs::PresetsClientAAPXS::setPresetIndex(int32_t index) {
    // we do not wait for the result, so no promise<T> involved
    uint32_t requestId = initiatorInstance->get_new_request_id(initiatorInstance);
    AAPXSRequestContext request{nullptr, nullptr, serialization, AAP_PRESETS_EXTENSION_URI, requestId, OPCODE_GET_PRESET_DATA};

    *(int32_t*) (serialization->data) = index;
    initiatorInstance->send_aapxs_request(initiatorInstance, &request);
}

// Strongly-typed service implementation (host extension functions)

void aap::xs::PresetsServiceAAPXS::notifyPresetLoaded() {
    uint32_t requestId = initiatorInstance->get_new_request_id(initiatorInstance);
    AAPXSRequestContext context{nullptr, nullptr, serialization, AAP_PRESETS_EXTENSION_URI, requestId, OPCODE_NOTIFY_PRESET_LOADED};
    initiatorInstance->send_aapxs_request(initiatorInstance, &context);
}

void aap::xs::PresetsServiceAAPXS::notifyPresetsUpdated() {
    uint32_t requestId = initiatorInstance->get_new_request_id(initiatorInstance);
    AAPXSRequestContext context{nullptr, nullptr, serialization, AAP_PRESETS_EXTENSION_URI, requestId, OPCODE_NOTIFY_PRESET_UPDATED};
    initiatorInstance->send_aapxs_request(initiatorInstance, &context);
}
