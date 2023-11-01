
#include "parameters-aapxs.h"
#include "aap/android-audio-plugin.h"
#include "aap/core/aapxs/aapxs-runtime.h"

// AAPXSDefinition_Parameters

void aap::xs::AAPXSDefinition_Parameters::aapxs_parameters_process_incoming_plugin_aapxs_request(
        struct AAPXSDefinition *feature, AAPXSRecipientInstance *aapxsInstance,
        AndroidAudioPlugin *plugin, AAPXSRequestContext *context) {
    auto ext = (aap_parameters_extension_t*) plugin->get_extension(plugin, AAP_PARAMETERS_EXTENSION_URI);
    switch (context->opcode) {
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
            break;
        case OPCODE_PARAMETERS_GET_PROPERTY: {
            int32_t parameterId = *((int32_t *) aapxsInstance->serialization->data);
            int32_t propertyId = *((int32_t *) aapxsInstance->serialization->data + 1);
            *((double *) aapxsInstance->serialization->data) =
                    ext != nullptr && ext->get_parameter_property ? ext->get_parameter_property(ext, plugin, parameterId, propertyId): 0.0;
            break;
        }
        case OPCODE_PARAMETERS_GET_ENUMERATION_COUNT: {
            int32_t parameterId = *((int32_t *) aapxsInstance->serialization->data);
            *((int32_t *) aapxsInstance->serialization->data) = ext != nullptr && ext->get_enumeration_count ? ext->get_enumeration_count(ext, plugin, parameterId) : 0;
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
            break;
    }
}

void aap::xs::AAPXSDefinition_Parameters::aapxs_parameters_process_incoming_host_aapxs_request(
        struct AAPXSDefinition *feature, AAPXSRecipientInstance *aapxsInstance,
        AndroidAudioPluginHost *host, AAPXSRequestContext *context) {
    // FIXME: implement
    assert(false);
}

void aap::xs::AAPXSDefinition_Parameters::aapxs_parameters_process_incoming_plugin_aapxs_reply(
        struct AAPXSDefinition *feature, AAPXSInitiatorInstance *aapxsInstance,
        AndroidAudioPlugin *plugin, AAPXSRequestContext *context) {
    // FIXME: implement
    assert(false);
}

void aap::xs::AAPXSDefinition_Parameters::aapxs_parameters_process_incoming_host_aapxs_reply(
        struct AAPXSDefinition *feature, AAPXSInitiatorInstance *aapxsInstance,
        AndroidAudioPluginHost *host, AAPXSRequestContext *context) {
    // FIXME: implement
    assert(false);
}

// AAPXSParametersClient

template<typename T>
void aap::xs::ParametersClientAAPXS::getParameterTypedCallback(void *callbackContext,
                                                             AndroidAudioPlugin *plugin,
                                                             int32_t requestId) {
    auto callbackData = (WithPromise<ParametersClientAAPXS, T>*) callbackContext;
    auto thiz = (ParametersClientAAPXS*) callbackData->context;
    T result = *(T*) (thiz->serialization->data);
    callbackData->promise->set_value(result);
}

template<typename T>
T aap::xs::ParametersClientAAPXS::callTypedParametersFunction(int32_t opcode) {
    // FIXME: use spinlock instead of std::promise and std::future, as getPresetCount() and getPresetIndex() must be RT_SAFE.
    std::promise<T> promise{};
    uint32_t requestId = initiatorInstance->get_new_request_id(initiatorInstance);
    auto future = promise.get_future();
    WithPromise<ParametersClientAAPXS, T> callbackData{this, &promise};
    AAPXSRequestContext request{getParameterTypedCallback<T>, &callbackData, serialization, AAP_PARAMETERS_EXTENSION_URI, requestId, opcode};

    initiatorInstance->send_aapxs_request(initiatorInstance, &request);

    future.wait();
    return future.get();
}

int32_t aap::xs::ParametersClientAAPXS::getParameterCount() {
    return callTypedParametersFunction<int32_t>(OPCODE_PARAMETERS_GET_PARAMETER_COUNT);
}

aap_parameter_info_t aap::xs::ParametersClientAAPXS::getParameter(int32_t index) {
    *((int32_t*) serialization->data) = index;
    return callTypedParametersFunction<aap_parameter_info_t>(OPCODE_PARAMETERS_GET_PARAMETER_COUNT);
}

double aap::xs::ParametersClientAAPXS::getProperty(int32_t index, int32_t propertyId) {
    *((int32_t*) serialization->data) = index;
    *((int32_t*) serialization->data + 1) = propertyId;
    return callTypedParametersFunction<double>(OPCODE_PARAMETERS_GET_PARAMETER_COUNT);
}

int32_t aap::xs::ParametersClientAAPXS::getEnumerationCount(int32_t index) {
    *((int32_t*) serialization->data) = index;
    return callTypedParametersFunction<int32_t>(OPCODE_PARAMETERS_GET_PARAMETER_COUNT);
}

aap_parameter_enum_t
aap::xs::ParametersClientAAPXS::getEnumeration(int32_t index, int32_t enumIndex) {
    *((int32_t*) serialization->data) = index;
    *((int32_t*) serialization->data + 1) = enumIndex;
    return callTypedParametersFunction<aap_parameter_enum_t>(OPCODE_PARAMETERS_GET_PARAMETER_COUNT);
}

void aap::xs::ParametersServiceAAPXS::notifyParametersChanged() {
    uint32_t requestId = initiatorInstance->get_new_request_id(initiatorInstance);
    AAPXSRequestContext context{nullptr, nullptr, serialization, AAP_PARAMETERS_EXTENSION_URI, requestId, OPCODE_NOTIFY_PARAMETERS_CHANGED};
    initiatorInstance->send_aapxs_request(initiatorInstance, &context);
}
