
#include <mutex>
#include "aap/core/aapxs/parameters-aapxs.h"
#include "aap/android-audio-plugin.h"
#include "aap/core/host/plugin-instance.h"
#include "aap/unstable/utility.h"

namespace {
void notify_parameters_changed(aap_parameters_host_extension_t* ext,
                               AndroidAudioPluginHost* host) {
    (void) ext;
    (void) host;
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
            *((int32_t*) request->serialization->data) = (ext && ext->get_parameter_count) ? ext->get_parameter_count(ext, plugin) : -1;
            request->serialization->data_size = sizeof(int32_t);
            aapxsInstance->send_aapxs_reply(aapxsInstance, request);
            break;
        case OPCODE_PARAMETERS_GET_PARAMETER:
            if (ext != nullptr && ext->get_parameter) {
                int32_t index = *((int32_t *) request->serialization->data);
                auto p = ext->get_parameter(ext, plugin, index);
                memcpy(request->serialization->data, (const void *) &p, sizeof(p));
            } else {
                memset(request->serialization->data, 0, sizeof(aap_parameter_info_t));
            }
            request->serialization->data_size = sizeof(aap_parameter_info_t);
            aapxsInstance->send_aapxs_reply(aapxsInstance, request);
            break;
        case OPCODE_PARAMETERS_GET_PROPERTY: {
            int32_t parameterId = *((int32_t *) request->serialization->data);
            int32_t propertyId = *((int32_t *) request->serialization->data + 1);
            *((double *) request->serialization->data) =
                    ext != nullptr && ext->get_parameter_property ? ext->get_parameter_property(ext, plugin, parameterId, propertyId): 0.0;
            request->serialization->data_size = sizeof(double);
            aapxsInstance->send_aapxs_reply(aapxsInstance, request);
            break;
        }
        case OPCODE_PARAMETERS_GET_ENUMERATION_COUNT: {
            int32_t parameterId = *((int32_t *) request->serialization->data);
            *((int32_t *) request->serialization->data) = ext != nullptr && ext->get_enumeration_count ? ext->get_enumeration_count(ext, plugin, parameterId) : 0;
            request->serialization->data_size = sizeof(int32_t);
            aapxsInstance->send_aapxs_reply(aapxsInstance, request);
            break;
        }
        case OPCODE_PARAMETERS_GET_ENUMERATION:
            if (ext != nullptr && ext->get_enumeration) {
                int32_t parameterId = *((int32_t *) request->serialization->data);
                int32_t enumIndex = *((int32_t *) request->serialization->data + 1);
                auto e = ext->get_enumeration(ext, plugin, parameterId, enumIndex);
                memcpy(request->serialization->data, (const void *) &e, sizeof(e));
            } else {
                memset(request->serialization->data, 0, sizeof(aap_parameter_enum_t));
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
    (void) feature;
    // One sender per plugin instance, owned through the instance's aapxs_context.
    static std::mutex creation_mutex;
    {
        const std::lock_guard<std::mutex> lock{creation_mutex};
        if (!aapxsInstance->aapxs_context)
            aapxsInstance->aapxs_context = new ParametersServiceAAPXS(aapxsInstance, serialization);
    }
    return AAPXSExtensionServiceProxy{aapxsInstance->aapxs_context, aapxs_parameters_as_host_extension};
}

void aap::xs::AAPXSDefinition_Parameters::aapxs_parameters_release_instance_context(
        struct AAPXSDefinition* feature, void* aapxsContext) {
    (void) feature;
    delete (ParametersServiceAAPXS*) aapxsContext;
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
    auto result = callAndWait<int32_t>(OPCODE_PARAMETERS_GET_PARAMETER_COUNT, nullptr, 0,
                                       [](AAPXSSerializationContext* ctx) -> int32_t {
        return getTypedResult<int32_t>(ctx);
    });
    return result.isOk() ? result.value : -1;
}

aap_parameter_info_t aap::xs::ParametersClientAAPXS::getParameter(int32_t index) {
    auto result = callAndWait<aap_parameter_info_t>(OPCODE_PARAMETERS_GET_PARAMETER, &index, sizeof(index),
                                                    [](AAPXSSerializationContext* ctx) -> aap_parameter_info_t {
        return getTypedResult<aap_parameter_info_t>(ctx);
    });
    return result.isOk() ? result.value : aap_parameter_info_t{};
}

double aap::xs::ParametersClientAAPXS::getProperty(int32_t index, int32_t propertyId) {
    int32_t payload[] {index, propertyId};
    auto result = callAndWait<double>(OPCODE_PARAMETERS_GET_PROPERTY, payload, sizeof(payload),
                                      [](AAPXSSerializationContext* ctx) -> double {
        return getTypedResult<double>(ctx);
    });
    return result.isOk() ? result.value : 0.0;
}

int32_t aap::xs::ParametersClientAAPXS::getEnumerationCount(int32_t index) {
    auto result = callAndWait<int32_t>(OPCODE_PARAMETERS_GET_ENUMERATION_COUNT, &index, sizeof(index),
                                       [](AAPXSSerializationContext* ctx) -> int32_t {
        return getTypedResult<int32_t>(ctx);
    });
    return result.isOk() ? result.value : 0;
}

aap_parameter_enum_t
aap::xs::ParametersClientAAPXS::getEnumeration(int32_t index, int32_t enumIndex) {
    int32_t payload[] {index, enumIndex};
    auto result = callAndWait<aap_parameter_enum_t>(OPCODE_PARAMETERS_GET_ENUMERATION, payload, sizeof(payload),
                                                    [](AAPXSSerializationContext* ctx) -> aap_parameter_enum_t {
        return getTypedResult<aap_parameter_enum_t>(ctx);
    });
    return result.isOk() ? result.value : aap_parameter_enum_t{};
}

// Context and errors use the same typed completion path as all other extensions.
int32_t aap::xs::ParametersClientAAPXS::getParameterAsync(int32_t index, ParameterCallback callback) {
    if (aap::RealtimeScope::isActive()) return -1;
    return callFunctionAsync(OPCODE_PARAMETERS_GET_PARAMETER, &index, sizeof(index),
        [this, index, callback](const std::string& error, AAPXSSerializationContext* ctx, void* pluginOrHost) {
            if (!callback)
                return;
            if (!error.empty()) {
                callback(this, pluginOrHost, index, {aap_parameter_info_t{}, error});
                return;
            }
            if (!ctx || !ctx->data || ctx->data_size < sizeof(aap_parameter_info_t) ||
                    ctx->data_capacity < sizeof(aap_parameter_info_t)) {
                callback(this, pluginOrHost, index, {aap_parameter_info_t{}, "short parameter reply"});
                return;
            }
            aap_parameter_info_t value{};
            memcpy(&value, ctx->data, sizeof(value));
            callback(this, pluginOrHost, index, {value, ""});
        }, sizeof(aap_parameter_info_t));
}

int32_t aap::xs::ParametersClientAAPXS::getEnumerationAsync(int32_t index, int32_t enumIndex,
                                                          EnumerationCallback callback) {
    if (aap::RealtimeScope::isActive()) return -1;
    int32_t payload[] {index, enumIndex};
    return callFunctionAsync(OPCODE_PARAMETERS_GET_ENUMERATION, payload, sizeof(payload),
        [this, index, enumIndex, callback](const std::string& error, AAPXSSerializationContext* ctx, void* pluginOrHost) {
            if (!callback)
                return;
            if (!error.empty()) {
                callback(this, pluginOrHost, index, enumIndex, {aap_parameter_enum_t{}, error});
                return;
            }
            if (!ctx || !ctx->data || ctx->data_size < sizeof(aap_parameter_enum_t) ||
                    ctx->data_capacity < sizeof(aap_parameter_enum_t)) {
                callback(this, pluginOrHost, index, enumIndex, {aap_parameter_enum_t{}, "short enumeration reply"});
                return;
            }
            aap_parameter_enum_t value{};
            memcpy(&value, ctx->data, sizeof(value));
            callback(this, pluginOrHost, index, enumIndex, {value, ""});
        }, sizeof(aap_parameter_enum_t));
}

void aap::xs::ParametersServiceAAPXS::notifyParametersChanged() {
    fireVoidFunctionAndForget(OPCODE_NOTIFY_PARAMETERS_CHANGED);
}
