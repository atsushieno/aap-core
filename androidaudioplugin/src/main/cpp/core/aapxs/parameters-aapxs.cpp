
#include "aap/core/aapxs/parameters-aapxs.h"
#include "aap/android-audio-plugin.h"
#include "aap/core/host/plugin-instance.h"
#include "../hosting/plugin-parameter-state.h"
#include "aap/unstable/utility.h"

namespace {
void notify_parameters_changed(aap_parameters_host_extension_t* ext,
                               AndroidAudioPluginHost* host) {
    (void) ext;
    auto* instance = (aap::RemotePluginInstance*) host->context;
    if (!instance)
        return;
    if (instance->parametersChangedHandler)
        instance->parametersChangedHandler(*instance);
}

aap_parameters_host_extension_t parameters_host_receiver{nullptr, notify_parameters_changed};
}

// AAPXSDefinition_Parameters

void aap::xs::AAPXSDefinition_Parameters::aapxs_parameters_process_incoming_plugin_aapxs_request(
        struct AAPXSDefinition *feature, AAPXSRecipientInstance *aapxsInstance,
        AndroidAudioPlugin *plugin, AAPXSRequestContext *request) {
    auto ext = (aap_parameters_extension_t*) plugin->get_extension(plugin, AAP_PARAMETERS_EXTENSION_URI);
    switch (request->opcode) {
        case OPCODE_PARAMETERS_GET_PARAMETER_COUNT:
            *((int32_t*) aapxsInstance->serialization->data) = (ext && ext->get_parameter_count) ? ext->get_parameter_count(ext, plugin) : -1;
            request->serialization->data_size = sizeof(int32_t);
            aapxsInstance->send_aapxs_reply(aapxsInstance, request);
            break;
        case OPCODE_PARAMETERS_GET_PARAMETER:
            if (ext != nullptr && ext->get_parameter) {
                int32_t index = *((int32_t *) aapxsInstance->serialization->data);
                auto p = ext->get_parameter(ext, plugin, index);
                memcpy(aapxsInstance->serialization->data, (const void *) &p, sizeof(p));
            } else {
                memset(aapxsInstance->serialization->data, 0, sizeof(aap_parameter_info_t));
            }
            request->serialization->data_size = sizeof(aap_parameter_info_t);
            aapxsInstance->send_aapxs_reply(aapxsInstance, request);
            break;
        case OPCODE_PARAMETERS_GET_PROPERTY: {
            int32_t parameterId = *((int32_t *) aapxsInstance->serialization->data);
            int32_t propertyId = *((int32_t *) aapxsInstance->serialization->data + 1);
            *((double *) aapxsInstance->serialization->data) =
                    ext != nullptr && ext->get_parameter_property ? ext->get_parameter_property(ext, plugin, parameterId, propertyId): 0.0;
            request->serialization->data_size = sizeof(double);
            aapxsInstance->send_aapxs_reply(aapxsInstance, request);
            break;
        }
        case OPCODE_PARAMETERS_GET_ENUMERATION_COUNT: {
            int32_t parameterId = *((int32_t *) aapxsInstance->serialization->data);
            *((int32_t *) aapxsInstance->serialization->data) = ext != nullptr && ext->get_enumeration_count ? ext->get_enumeration_count(ext, plugin, parameterId) : 0;
            request->serialization->data_size = sizeof(int32_t);
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
            request->serialization->data_size = sizeof(aap_parameter_enum_t);
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
    switch (request->opcode) {
        case OPCODE_NOTIFY_PARAMETERS_CHANGED: {
            if (ext)
                ext->notify_parameters_changed(ext, host);
            //else {
            // FIXME: log warning?
            //}
            aapxsInstance->send_aapxs_reply(aapxsInstance, request);
            break;
        }
        default:
            // FIXME: log warning?
            break;
    }
}

void aap::xs::AAPXSDefinition_Parameters::aapxs_parameters_process_incoming_plugin_aapxs_reply(
        struct AAPXSDefinition *feature, AAPXSInitiatorInstance *aapxsInstance,
        AndroidAudioPlugin *plugin, AAPXSRequestContext *request) {
    if (request->callback != nullptr)
        request->callback(request->callback_user_data, plugin);
}

void aap::xs::AAPXSDefinition_Parameters::aapxs_parameters_process_incoming_host_aapxs_reply(
        struct AAPXSDefinition *feature, AAPXSInitiatorInstance *aapxsInstance,
        AndroidAudioPluginHost *host, AAPXSRequestContext *request) {
    if (request->callback != nullptr)
        request->callback(request->callback_user_data, host);
}

AAPXSExtensionClientProxy aap::xs::AAPXSDefinition_Parameters::aapxs_parameters_get_plugin_proxy(
        struct AAPXSDefinition *feature, AAPXSInitiatorInstance *aapxsInstance,
        AAPXSSerializationContext *serialization) {
    (void) serialization;
    auto client = (AAPXSDefinition_Parameters*) feature->aapxs_context;
    auto* instance = (aap::PluginInstance*) aapxsInstance->host_context;
    client->client_proxy = AAPXSExtensionClientProxy{
            instance ? instance->getStandardExtensions().asParametersExtension() : nullptr,
            aapxs_parameters_as_plugin_extension};
    return client->client_proxy;
}

AAPXSExtensionServiceProxy aap::xs::AAPXSDefinition_Parameters::aapxs_parameters_get_host_proxy(
        struct AAPXSDefinition *feature, AAPXSInitiatorInstance *aapxsInstance,
        AAPXSSerializationContext *serialization) {
    auto service = (AAPXSDefinition_Parameters*) feature->aapxs_context;
    service->typed_service = std::make_unique<ParametersServiceAAPXS>(aapxsInstance, serialization);
    service->service_proxy.aapxs_context = service->typed_service.get();
    service->service_proxy.as_host_extension = aapxs_parameters_as_host_extension;
    return service->service_proxy;
}

AAPXSExtensionHostReceiver
aap::xs::AAPXSDefinition_Parameters::aapxs_parameters_get_host_receiver(
        struct AAPXSDefinition *feature,
        AAPXSRecipientInstance *aapxsInstance,
        AndroidAudioPluginHost *host) {
    (void) feature;
    (void) aapxsInstance;
    (void) host;
    return AAPXSExtensionHostReceiver{nullptr, aapxs_parameters_as_host_receiver};
}

void* aap::xs::AAPXSDefinition_Parameters::aapxs_parameters_as_host_receiver(
        AAPXSExtensionHostReceiver *receiver) {
    (void) receiver;
    return &parameters_host_receiver;
}

// AAPXSParametersClient

int32_t aap::xs::ParametersClientAAPXS::getParameterCount() {
    serialization->data_size = 0;
    auto result = callAndWait<int32_t>(OPCODE_PARAMETERS_GET_PARAMETER_COUNT,
                                       [](AAPXSSerializationContext* ctx) -> int32_t {
        return getTypedResult<int32_t>(ctx);
    });
    return result.isOk() ? result.value : -1;
}

aap_parameter_info_t aap::xs::ParametersClientAAPXS::getParameter(int32_t index) {
    *((int32_t*) serialization->data) = index;
    serialization->data_size = sizeof(int32_t);
    auto result = callAndWait<aap_parameter_info_t>(OPCODE_PARAMETERS_GET_PARAMETER,
                                                    [](AAPXSSerializationContext* ctx) -> aap_parameter_info_t {
        return getTypedResult<aap_parameter_info_t>(ctx);
    });
    return result.isOk() ? result.value : aap_parameter_info_t{};
}

double aap::xs::ParametersClientAAPXS::getProperty(int32_t index, int32_t propertyId) {
    *((int32_t*) serialization->data) = index;
    *((int32_t*) serialization->data + 1) = propertyId;
    serialization->data_size = sizeof(int32_t) * 2;
    auto result = callAndWait<double>(OPCODE_PARAMETERS_GET_PROPERTY,
                                      [](AAPXSSerializationContext* ctx) -> double {
        return getTypedResult<double>(ctx);
    });
    return result.isOk() ? result.value : 0.0;
}

int32_t aap::xs::ParametersClientAAPXS::getEnumerationCount(int32_t index) {
    *((int32_t*) serialization->data) = index;
    serialization->data_size = sizeof(int32_t);
    auto result = callAndWait<int32_t>(OPCODE_PARAMETERS_GET_ENUMERATION_COUNT,
                                       [](AAPXSSerializationContext* ctx) -> int32_t {
        return getTypedResult<int32_t>(ctx);
    });
    return result.isOk() ? result.value : 0;
}

aap_parameter_enum_t
aap::xs::ParametersClientAAPXS::getEnumeration(int32_t index, int32_t enumIndex) {
    *((int32_t*) serialization->data) = index;
    *((int32_t*) serialization->data + 1) = enumIndex;
    serialization->data_size = sizeof(int32_t) * 2;
    auto result = callAndWait<aap_parameter_enum_t>(OPCODE_PARAMETERS_GET_ENUMERATION,
                                                    [](AAPXSSerializationContext* ctx) -> aap_parameter_enum_t {
        return getTypedResult<aap_parameter_enum_t>(ctx);
    });
    return result.isOk() ? result.value : aap_parameter_enum_t{};
}

void aap::xs::ParametersClientAAPXS::completeWithParameterCallback (void* callbackData, void* pluginOrHost) {
    auto cb = (CallbackData*) callbackData;
    auto thiz = (ParametersClientAAPXS *) cb->context;
    auto result = thiz->getTypedResult<aap_parameter_info_t>(thiz->serialization);
    ((aapxs_async_get_parameter_callback) cb->callback) (thiz, pluginOrHost, cb->index, result);
    cb->context = nullptr;
}

void aap::xs::ParametersClientAAPXS::completeWithEnumCallback (void* callbackData, void* pluginOrHost) {
    auto cb = (CallbackData*) callbackData;
    auto thiz = (ParametersClientAAPXS *) cb->context;
    auto result = getTypedResult<aap_parameter_enum_t>(thiz->serialization);
    ((aapxs_async_get_enumeration_callback) cb->callback) (thiz, pluginOrHost, cb->index, cb->enum_index, result);
    cb->context = nullptr;
}


int32_t
aap::xs::ParametersClientAAPXS::getParameterAsync(int32_t index,
                                                  aapxs_async_get_parameter_callback* callback) {
    *((int32_t*) serialization->data) = index;
    serialization->data_size = sizeof(int32_t);

    uint32_t requestId = aapxs_instance->get_new_request_id(aapxs_instance);
    CallbackData *callbackData = nullptr;
    for (size_t i = 0; i < UINT8_MAX; i++)
        if (pending_calls[i].context == nullptr)
            callbackData = pending_calls + i;
    if (!callbackData) {
        AAP_ASSERT_FALSE;
        return -1;
    }
    *callbackData = {this, callback, index, 0};
    AAPXSRequestContext request{completeWithParameterCallback, callbackData, serialization,
                                0, AAP_PARAMETERS_EXTENSION_URI, requestId,
                                OPCODE_PARAMETERS_GET_PARAMETER};

    aapxs_instance->send_aapxs_request(aapxs_instance, &request);

    return requestId;
}

int32_t aap::xs::ParametersClientAAPXS::getEnumerationAsync(int32_t index, int32_t enumIndex,
                                                            aapxs_async_get_enumeration_callback* callback) {
    *((int32_t*) serialization->data) = index;
    *((int32_t*) serialization->data + 1) = enumIndex;
    serialization->data_size = sizeof(int32_t) * 2;

    uint32_t requestId = aapxs_instance->get_new_request_id(aapxs_instance);
    CallbackData *callbackData = nullptr;
    for (size_t i = 0; i < UINT8_MAX; i++)
        if (pending_calls[i].context == nullptr)
            callbackData = pending_calls + i;
    if (!callbackData) {
        AAP_ASSERT_FALSE;
        return -1;
    }
    *callbackData = {this, callback, index, enumIndex};
    AAPXSRequestContext request{completeWithEnumCallback, callbackData, serialization,
                                0, AAP_PARAMETERS_EXTENSION_URI, requestId, OPCODE_PARAMETERS_GET_ENUMERATION};

    aapxs_instance->send_aapxs_request(aapxs_instance, &request);

    return requestId;
}

void aap::xs::ParametersServiceAAPXS::notifyParametersChanged() {
    fireVoidFunctionAndForget(OPCODE_NOTIFY_PARAMETERS_CHANGED);
}
