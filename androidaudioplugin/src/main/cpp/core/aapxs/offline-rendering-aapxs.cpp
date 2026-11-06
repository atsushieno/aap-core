#include "aap/core/aapxs/offline-rendering-aapxs.h"
#include "aap/core/host/plugin-instance.h"

namespace {
    int32_t setOfflineModeOnPlugin(AAPXSRecipientInstance* aapxsInstance, AndroidAudioPlugin* plugin, int32_t mode) {
        // The recipient's host context is always the LocalPluginInstance (see setupAAPXSInstances()).
        auto instance = static_cast<aap::LocalPluginInstance*>(aapxsInstance->host_context);
        if (instance->getInstanceState() == aap::PLUGIN_INSTANTIATION_STATE_ACTIVE)
            return AAP_OFFLINE_RENDERING_RESULT_GENERIC_FAILURE;
        if (mode != AAP_OFFLINE_RENDERING_MODE_REALTIME && mode != AAP_OFFLINE_RENDERING_MODE_OFFLINE)
            return AAP_OFFLINE_RENDERING_RESULT_GENERIC_FAILURE;
        auto ext = (aap_offline_rendering_extension_t*) plugin->get_extension(plugin, AAP_OFFLINE_RENDERING_EXTENSION_URI);
        if (!ext || !ext->set_offline_mode)
            return AAP_OFFLINE_RENDERING_RESULT_GENERIC_FAILURE;
        auto result = ext->set_offline_mode(ext, plugin, (aap_offline_rendering_mode_t) mode);
        if (result == AAP_OFFLINE_RENDERING_RESULT_OK)
            instance->setOfflineRendering(mode == AAP_OFFLINE_RENDERING_MODE_OFFLINE);
        return result;
    }

    aap_offline_rendering_result_t deserializeResult(AAPXSSerializationContext* ctx) {
        int32_t result = AAP_OFFLINE_RENDERING_RESULT_GENERIC_FAILURE;
        if (ctx && ctx->data && ctx->data_size >= sizeof(result) && ctx->data_capacity >= sizeof(result))
            memcpy(&result, ctx->data, sizeof(result));
        return result == AAP_OFFLINE_RENDERING_RESULT_OK ? AAP_OFFLINE_RENDERING_RESULT_OK : AAP_OFFLINE_RENDERING_RESULT_GENERIC_FAILURE;
    }
}

void aap::xs::AAPXSDefinition_OfflineRendering::aapxs_offline_rendering_process_incoming_plugin_aapxs_request(
        struct AAPXSDefinition *feature, AAPXSRecipientInstance *aapxsInstance,
        AndroidAudioPlugin *plugin, AAPXSRequestContext *request) {
    (void) feature;
    auto* data = request->serialization;
    switch (request->opcode) {
        case OPCODE_SET_OFFLINE_MODE: {
            int32_t result = AAP_OFFLINE_RENDERING_RESULT_GENERIC_FAILURE;
            // Like other extensions, do not depend on the request size; it is not available on
            // every transport.
            if (data && data->data && data->data_capacity >= sizeof(int32_t)) {
                int32_t mode;
                memcpy(&mode, data->data, sizeof(mode));
                result = setOfflineModeOnPlugin(aapxsInstance, plugin, mode);
                memcpy(data->data, &result, sizeof(result));
                data->data_size = sizeof(result);
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

void aap::xs::AAPXSDefinition_OfflineRendering::aapxs_offline_rendering_process_incoming_plugin_aapxs_reply(
        struct AAPXSDefinition *feature, AAPXSInitiatorInstance *aapxsInstance,
        AndroidAudioPlugin *plugin, AAPXSRequestContext *request) {
    (void) feature;
    (void) aapxsInstance;
    if (request->callback != nullptr)
        request->callback(request->callback_user_data, plugin);
}

void aap::xs::AAPXSDefinition_OfflineRendering::aapxs_offline_rendering_process_incoming_host_aapxs_reply(
        struct AAPXSDefinition *feature, AAPXSInitiatorInstance *aapxsInstance,
        AndroidAudioPluginHost *host, AAPXSRequestContext *request) {
    (void) feature;
    (void) aapxsInstance;
    if (request->callback != nullptr)
        request->callback(request->callback_user_data, host);
}

AAPXSExtensionClientProxy
aap::xs::AAPXSDefinition_OfflineRendering::aapxs_offline_rendering_get_plugin_proxy(
        struct AAPXSDefinition *feature, AAPXSInitiatorInstance *aapxsInstance,
        AAPXSSerializationContext *serialization) {
    (void) feature;
    (void) serialization;
    return AAPXSExtensionClientProxy{aapxsInstance->aapxs_context, aapxs_offline_rendering_as_plugin_extension};
}

// Strongly-typed client implementation (plugin extension functions)

aap::Result<aap_offline_rendering_result_t> aap::xs::OfflineRenderingClientAAPXS::setOfflineMode(aap_offline_rendering_mode_t mode) {
    int32_t payload = mode;
    return callAndWait<aap_offline_rendering_result_t>(OPCODE_SET_OFFLINE_MODE, &payload, sizeof(payload), deserializeResult, sizeof(int32_t));
}

int32_t aap::xs::OfflineRenderingClientAAPXS::setOfflineModeAsync(aap_offline_rendering_mode_t mode, std::function<void(Result<aap_offline_rendering_result_t>)> callback) {
    int32_t payload = mode;
    return callFunctionAsync(OPCODE_SET_OFFLINE_MODE, &payload, sizeof(payload),
                             [callback = std::move(callback)](const std::string& error, AAPXSSerializationContext* ctx, void*) {
        if (!callback)
            return;
        if (!error.empty())
            callback(Result<aap_offline_rendering_result_t>{AAP_OFFLINE_RENDERING_RESULT_GENERIC_FAILURE, error});
        else
            callback(Result<aap_offline_rendering_result_t>{deserializeResult(ctx), ""});
    }, sizeof(int32_t));
}
