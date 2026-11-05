#ifndef AAP_CORE_OFFLINE_RENDERING_AAPXS_H
#define AAP_CORE_OFFLINE_RENDERING_AAPXS_H

#include <functional>
#include "aap/aapxs.h"
#include "../../ext/offline-rendering.h"
#include "typed-aapxs.h"

// plugin extension opcodes

// Request: int32_t aap_offline_rendering_mode_t. Reply: int32_t aap_offline_rendering_result_t.
const int32_t OPCODE_SET_OFFLINE_MODE = 1;

// There is no host extension.

const int32_t OFFLINE_RENDERING_SHARED_MEMORY_SIZE = sizeof(int32_t);

namespace aap::xs {

    class OfflineRenderingClientAAPXS : public TypedAAPXS {
        static aap_offline_rendering_result_t staticSetOfflineMode(aap_offline_rendering_extension_t* ext, AndroidAudioPlugin*, aap_offline_rendering_mode_t mode) {
            auto result = ((OfflineRenderingClientAAPXS*) ext->aapxs_context)->setOfflineMode(mode);
            return result.isOk() ? result.value : AAP_OFFLINE_RENDERING_RESULT_GENERIC_FAILURE;
        }
        aap_offline_rendering_extension_t as_plugin_extension{this, staticSetOfflineMode};

    public:
        OfflineRenderingClientAAPXS(AAPXSInitiatorInstance* initiatorInstance, AAPXSSerializationContext* serialization)
                : TypedAAPXS(AAP_OFFLINE_RENDERING_EXTENSION_URI, initiatorInstance, serialization) {
        }

        // Blocking-sync. An error means the request failed, not that the plugin refused the mode.
        Result<aap_offline_rendering_result_t> setOfflineMode(aap_offline_rendering_mode_t mode);
        // Asynchronous. Returns the request id; the callback fires exactly once on reply/timeout/death.
        int32_t setOfflineModeAsync(aap_offline_rendering_mode_t mode, std::function<void(Result<aap_offline_rendering_result_t>)> callback);

        aap_offline_rendering_extension_t* asPluginExtension() { return &as_plugin_extension; }
    };

    class AAPXSDefinition_OfflineRendering : public AAPXSDefinitionWrapper {

        static void aapxs_offline_rendering_process_incoming_plugin_aapxs_request(
                struct AAPXSDefinition* feature,
                AAPXSRecipientInstance* aapxsInstance,
                AndroidAudioPlugin* plugin,
                AAPXSRequestContext* request);
        static void aapxs_offline_rendering_process_incoming_plugin_aapxs_reply(
                struct AAPXSDefinition* feature,
                AAPXSInitiatorInstance* aapxsInstance,
                AndroidAudioPlugin* plugin,
                AAPXSRequestContext* request);
        static void aapxs_offline_rendering_process_incoming_host_aapxs_reply(
                struct AAPXSDefinition* feature,
                AAPXSInitiatorInstance* aapxsInstance,
                AndroidAudioPluginHost* host,
                AAPXSRequestContext* request);

        static AAPXSExtensionClientProxy aapxs_offline_rendering_get_plugin_proxy(
                struct AAPXSDefinition* feature,
                AAPXSInitiatorInstance* aapxsInstance,
                AAPXSSerializationContext* serialization);

        static void* aapxs_offline_rendering_as_plugin_extension(AAPXSExtensionClientProxy* proxy) {
            auto* client = (OfflineRenderingClientAAPXS*) proxy->aapxs_context;
            return client ? client->asPluginExtension() : nullptr;
        }

        // Request flags stay default: the request changes plugin state, so it runs under control exclusion.
        AAPXSDefinition aapxs_offline_rendering{
            this,
            AAP_OFFLINE_RENDERING_EXTENSION_URI,
            OFFLINE_RENDERING_SHARED_MEMORY_SIZE,
            aapxs_offline_rendering_process_incoming_plugin_aapxs_request,
            nullptr,
            aapxs_offline_rendering_process_incoming_plugin_aapxs_reply,
            aapxs_offline_rendering_process_incoming_host_aapxs_reply,
            aapxs_offline_rendering_get_plugin_proxy,
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            initializeTypedAAPXSInitiator<OfflineRenderingClientAAPXS>,
            nullptr,
            releaseTypedAAPXSInitiator,
            nullptr
        };

    public:
        AAPXSDefinition& asPublic() override {
            return aapxs_offline_rendering;
        }
    };
}

#endif //AAP_CORE_OFFLINE_RENDERING_AAPXS_H
