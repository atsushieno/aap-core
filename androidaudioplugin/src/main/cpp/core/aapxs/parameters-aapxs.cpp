
#include "aap/core/aapxs/parameters-aapxs.h"
#include "aap/android-audio-plugin.h"

// AAPXSDefinition_Parameters

void aap::xs::AAPXSDefinition_Parameters::aapxs_parameters_process_incoming_plugin_aapxs_request(
        struct AAPXSDefinition *feature, AAPXSRecipientInstance *aapxsInstance,
        AndroidAudioPlugin *plugin, AAPXSRequestContext *request) {
    auto ext = (aap_parameters_extension_t*) plugin->get_extension(plugin, AAP_PARAMETERS_EXTENSION_URI);
    if (!ext)
        return; // FIXME: should there be any global error handling?
    switch (request->opcode) {
        case OPCODE_PARAMETERS_GET_PARAMETER_COUNT:
            *((int32_t*) aapxsInstance->serialization->data) = (ext && ext->get_parameter_count) ? ext->get_parameter_count(ext, plugin) : -1;
            break;
        case OPCODE_PARAMETERS_GET_PARAMETER:
            if (ext != nullptr && ext->get_parameter) {
                int32_t index = *((int32_t *) aapxsInstance->serialization->data);
                auto p = ext->get_parameter(ext, plugin, index);
                memcpy(aapxsInstance->serialization->data, (const void *) &p, sizeof(p));
            } else {
                memset(aapxsInstance->serialization->data, 0, sizeof(aap_parameter_info_t));
            }
            aapxsInstance->send_aapxs_reply(aapxsInstance, request);
            break;
        case OPCODE_PARAMETERS_GET_PROPERTY: {
            int32_t parameterId = *((int32_t *) aapxsInstance->serialization->data);
            int32_t propertyId = *((int32_t *) aapxsInstance->serialization->data + 1);
            *((double *) aapxsInstance->serialization->data) =
                    ext != nullptr && ext->get_parameter_property ? ext->get_parameter_property(ext, plugin, parameterId, propertyId): 0.0;
            aapxsInstance->send_aapxs_reply(aapxsInstance, request);
            break;
        }
        case OPCODE_PARAMETERS_GET_ENUMERATION_COUNT: {
            int32_t parameterId = *((int32_t *) aapxsInstance->serialization->data);
            *((int32_t *) aapxsInstance->serialization->data) = ext != nullptr && ext->get_enumeration_count ? ext->get_enumeration_count(ext, plugin, parameterId) : 0;
            aapxsInstance->send_aapxs_reply(aapxsInstance, request);
            break;
        }
        case OPCODE_PARAMETERS_GET_ENUMERATION:
            if (ext != nullptr && ext->get_enumeration) {
                int32_t parameterId = *((int32_t *) aapxsInstance->serialization->data);
                int32_t enumIndex = *((int32_t *) aapxsInstance->serialization->data + 1);
                auto e = ext->get_enumeration(ext, plugin, parameterId, enumIndex);
                memcpy(aapxsInstance->serialization->data, (const void *) &e, sizeof(e));
            } else {
                memset(aapxsInstance->serialization->data, 0, sizeof(aap_parameter_enum_t));
            }
            aapxsInstance->send_aapxs_reply(aapxsInstance, request);
            break;
        default:
            // FIXME: log warning?
            break;
    }
}

void aap::xs::AAPXSDefinition_Parameters::aapxs_parameters_process_incoming_host_aapxs_request(
        struct AAPXSDefinition *feature, AAPXSRecipientInstance *aapxsInstance,
        AndroidAudioPluginHost *host, AAPXSRequestContext *request) {
    auto ext = (aap_parameters_host_extension_t*) host->get_extension(host, AAP_PARAMETERS_EXTENSION_URI);
    if (!ext)
        return; // FIXME: should there be any global error handling?
    switch (request->opcode) {
        case OPCODE_NOTIFY_PARAMETERS_CHANGED:
            ext->notify_parameters_changed(ext, host);
            aapxsInstance->send_aapxs_reply(aapxsInstance, request);
            break;
        default:
            // FIXME: log warning?
            break;
    }
}

void aap::xs::AAPXSDefinition_Parameters::aapxs_parameters_process_incoming_plugin_aapxs_reply(
        struct AAPXSDefinition *feature, AAPXSInitiatorInstance *aapxsInstance,
        AndroidAudioPlugin *plugin, AAPXSRequestContext *request) {
    if (request->callback != nullptr)
        request->callback(request->callback_user_data, plugin, request->request_id);
}

void aap::xs::AAPXSDefinition_Parameters::aapxs_parameters_process_incoming_host_aapxs_reply(
        struct AAPXSDefinition *feature, AAPXSInitiatorInstance *aapxsInstance,
        AndroidAudioPluginHost *host, AAPXSRequestContext *request) {
    if (request->callback != nullptr)
        request->callback(request->callback_user_data, host, request->request_id);
}

AAPXSExtensionClientProxy aap::xs::AAPXSDefinition_Parameters::aapxs_parameters_get_plugin_proxy(
        struct AAPXSDefinition *feature, AAPXSInitiatorInstance *aapxsInstance,
        AAPXSSerializationContext *serialization) {
    auto client = (AAPXSDefinition_Parameters*) feature->aapxs_context;
    if (!client->typed_client)
        client->typed_client = std::make_unique<ParametersClientAAPXS>(aapxsInstance, serialization);
    *client->typed_client = ParametersClientAAPXS(aapxsInstance, serialization);
    client->client_proxy = AAPXSExtensionClientProxy{client->typed_client.get(), aapxs_parameters_as_plugin_extension};
    return client->client_proxy;
}

AAPXSExtensionServiceProxy aap::xs::AAPXSDefinition_Parameters::aapxs_parameters_get_host_proxy(
        struct AAPXSDefinition *feature, AAPXSInitiatorInstance *aapxsInstance,
        AAPXSSerializationContext *serialization) {
    auto service = (AAPXSDefinition_Parameters*) feature->aapxs_context;
    if (!service->typed_service)
        service->typed_service = std::make_unique<ParametersServiceAAPXS>(aapxsInstance, serialization);
    *service->typed_service = ParametersServiceAAPXS(aapxsInstance, serialization);
    service->service_proxy = AAPXSExtensionServiceProxy{&service->typed_service, aapxs_parameters_as_host_extension};
    return service->service_proxy;
}

// AAPXSParametersClient

int32_t aap::xs::ParametersClientAAPXS::getParameterCount() {
    return callTypedFunctionSynchronously<int32_t>(OPCODE_PARAMETERS_GET_PARAMETER_COUNT);
}

aap_parameter_info_t aap::xs::ParametersClientAAPXS::getParameter(int32_t index) {
    *((int32_t*) serialization->data) = index;
    return callTypedFunctionSynchronously<aap_parameter_info_t>(
            OPCODE_PARAMETERS_GET_PARAMETER);
}

double aap::xs::ParametersClientAAPXS::getProperty(int32_t index, int32_t propertyId) {
    *((int32_t*) serialization->data) = index;
    *((int32_t*) serialization->data + 1) = propertyId;
    return callTypedFunctionSynchronously<double>(OPCODE_PARAMETERS_GET_PROPERTY);
}

int32_t aap::xs::ParametersClientAAPXS::getEnumerationCount(int32_t index) {
    *((int32_t*) serialization->data) = index;
    return callTypedFunctionSynchronously<int32_t>(OPCODE_PARAMETERS_GET_ENUMERATION_COUNT);
}

aap_parameter_enum_t
aap::xs::ParametersClientAAPXS::getEnumeration(int32_t index, int32_t enumIndex) {
    *((int32_t*) serialization->data) = index;
    *((int32_t*) serialization->data + 1) = enumIndex;
    return callTypedFunctionSynchronously<aap_parameter_enum_t>(
            OPCODE_PARAMETERS_GET_ENUMERATION);
}

void aap::xs::ParametersClientAAPXS::completeWithParameterCallback (void* callbackData, void* pluginOrHost, int32_t requestId) {
    auto cb = (CallbackData*) callbackData;
    auto thiz = (ParametersClientAAPXS *) cb->context;
    auto result = thiz->getTypedResult<aap_parameter_info_t>(thiz->serialization);
    ((aapxs_async_get_parameter_callback) cb->callback) (thiz, pluginOrHost, requestId, cb->index, result);
    cb->context = nullptr;
}

void aap::xs::ParametersClientAAPXS::completeWithEnumCallback (void* callbackData, void* pluginOrHost, int32_t requestId) {
    auto cb = (CallbackData*) callbackData;
    auto thiz = (ParametersClientAAPXS *) cb->context;
    auto result = getTypedResult<aap_parameter_enum_t>(thiz->serialization);
    ((aapxs_async_get_enumeration_callback) cb->callback) (thiz, pluginOrHost, requestId, cb->index, cb->enum_index, result);
    cb->context = nullptr;
}


int32_t
aap::xs::ParametersClientAAPXS::getParameterAsync(int32_t index,
                                                  aapxs_async_get_parameter_callback* callback) {
    *((int32_t*) serialization->data) = index;

    uint32_t requestId = aapxs_instance->get_new_request_id(aapxs_instance);
    CallbackData *callbackData = nullptr;
    for (size_t i = 0; i < UINT8_MAX; i++)
        if (pending_calls[i].context == nullptr)
            callbackData = pending_calls + i;
    assert(callbackData);
    *callbackData = {this, callback, index, 0};
    AAPXSRequestContext request{completeWithParameterCallback, callbackData, serialization,
                                0, AAP_PARAMETERS_EXTENSION_URI, requestId, OPCODE_PARAMETERS_GET_PARAMETER};

    aapxs_instance->send_aapxs_request(aapxs_instance, &request);

    return requestId;
}

int32_t aap::xs::ParametersClientAAPXS::getEnumerationAsync(int32_t index, int32_t enumIndex,
                                                            aapxs_async_get_enumeration_callback* callback) {
    *((int32_t*) serialization->data) = index;

    uint32_t requestId = aapxs_instance->get_new_request_id(aapxs_instance);
    CallbackData *callbackData = nullptr;
    for (size_t i = 0; i < UINT8_MAX; i++)
        if (pending_calls[i].context == nullptr)
            callbackData = pending_calls + i;
    assert(callbackData);
    *callbackData = {this, callback, index, enumIndex};
    AAPXSRequestContext request{completeWithEnumCallback, callbackData, serialization,
                                0, AAP_PARAMETERS_EXTENSION_URI, requestId, OPCODE_PARAMETERS_GET_ENUMERATION};

    aapxs_instance->send_aapxs_request(aapxs_instance, &request);

    return requestId;
}

void aap::xs::ParametersServiceAAPXS::notifyParametersChanged() {
    callVoidFunctionSynchronously(OPCODE_NOTIFY_PARAMETERS_CHANGED);
}
