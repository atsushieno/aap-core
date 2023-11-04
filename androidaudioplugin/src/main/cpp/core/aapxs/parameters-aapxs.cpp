
#include "aap/core/aapxs/parameters-aapxs.h"
#include "aap/android-audio-plugin.h"
#include "aap/core/aapxs/aapxs-runtime.h"

// AAPXSDefinition_Parameters

void aap::xs::AAPXSDefinition_Parameters::aapxs_parameters_process_incoming_plugin_aapxs_request(
        struct AAPXSDefinition *feature, AAPXSRecipientInstance *aapxsInstance,
        AndroidAudioPlugin *plugin, AAPXSRequestContext *request) {
    auto ext = (aap_parameters_extension_t*) plugin->get_extension(plugin, AAP_PARAMETERS_EXTENSION_URI);
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

// AAPXSParametersClient

int32_t aap::xs::ParametersClientAAPXS::getParameterCount() {
    return callTypedFunctionSynchronously<int32_t>(OPCODE_PARAMETERS_GET_PARAMETER_COUNT);
}

aap_parameter_info_t aap::xs::ParametersClientAAPXS::getParameter(int32_t index) {
    *((int32_t*) serialization->data) = index;
    return callTypedFunctionSynchronously<aap_parameter_info_t>(
            OPCODE_PARAMETERS_GET_PARAMETER_COUNT);
}

double aap::xs::ParametersClientAAPXS::getProperty(int32_t index, int32_t propertyId) {
    *((int32_t*) serialization->data) = index;
    *((int32_t*) serialization->data + 1) = propertyId;
    return callTypedFunctionSynchronously<double>(OPCODE_PARAMETERS_GET_PARAMETER_COUNT);
}

int32_t aap::xs::ParametersClientAAPXS::getEnumerationCount(int32_t index) {
    *((int32_t*) serialization->data) = index;
    return callTypedFunctionSynchronously<int32_t>(OPCODE_PARAMETERS_GET_PARAMETER_COUNT);
}

aap_parameter_enum_t
aap::xs::ParametersClientAAPXS::getEnumeration(int32_t index, int32_t enumIndex) {
    *((int32_t*) serialization->data) = index;
    *((int32_t*) serialization->data + 1) = enumIndex;
    return callTypedFunctionSynchronously<aap_parameter_enum_t>(
            OPCODE_PARAMETERS_GET_PARAMETER_COUNT);
}

void aap::xs::ParametersServiceAAPXS::notifyParametersChanged() {
    callVoidFunctionSynchronously(OPCODE_NOTIFY_PARAMETERS_CHANGED);
}
