#ifndef AAP_CORE_PRESETS_AAPXS_H
#define AAP_CORE_PRESETS_AAPXS_H

#include <functional>
#include <future>
#include "typed-aapxs.h"
#include "../../ext/presets.h"

// plugin extension opcodes
const int32_t OPCODE_GET_PRESET_COUNT = 1;
const int32_t OPCODE_GET_PRESET_DATA = 2;
const int32_t OPCODE_GET_PRESET_INDEX = 3;
const int32_t OPCODE_SET_PRESET_INDEX = 4;

// host extension opcodes
const int32_t OPCODE_NOTIFY_PRESET_LOADED = -1;
const int32_t OPCODE_NOTIFY_PRESETS_UPDATED = -2;

const int32_t PRESETS_SHARED_MEMORY_SIZE = sizeof(aap_preset_t) + sizeof(int32_t);

namespace aap::xs {

    class PresetsClientAAPXS : public TypedAAPXS {
        // extension proxy support
        static int32_t staticGetPresetCount(aap_presets_extension_t* ext, AndroidAudioPlugin* plugin) {
            return ((PresetsClientAAPXS*) ext->aapxs_context)->getPresetCount();
        }
        static void staticGetPreset(aap_presets_extension_t* ext, AndroidAudioPlugin* plugin, int32_t index, aap_preset_t *preset, aapxs_completion_callback aapxsCallback, void* callbackData) {
            ((PresetsClientAAPXS*) ext->aapxs_context)->getPreset(index, *preset);
            if (aapxsCallback)
                aapxsCallback(callbackData, plugin, 0); // do we need precise requestId?
        }
        static int32_t staticGetPresetIndex(aap_presets_extension_t* ext, AndroidAudioPlugin* plugin) {
            return ((PresetsClientAAPXS*) ext->aapxs_context)->getPresetIndex();
        }
        static void staticSetPresetIndex(aap_presets_extension_t* ext, AndroidAudioPlugin* plugin, int32_t index) {
            ((PresetsClientAAPXS*) ext->aapxs_context)->setPresetIndex(index);
        }
        aap_presets_extension_t as_plugin_extension{this,
                                                    staticGetPresetCount,
                                                    staticGetPreset,
                                                    staticGetPresetIndex,
                                                    staticSetPresetIndex};

    public:
        PresetsClientAAPXS(AAPXSInitiatorInstance* initiatorInstance, AAPXSSerializationContext* serialization)
                : TypedAAPXS(AAP_PRESETS_EXTENSION_URI, initiatorInstance, serialization) {
        }

        int32_t getPresetCount();
        void getPreset(int32_t index, aap_preset_t& preset);
        int32_t getPresetIndex();
        void setPresetIndex(int32_t index);

        aap_presets_extension_t* asPluginExtension() { return &as_plugin_extension; }
    };

    class PresetsServiceAAPXS : public TypedAAPXS {
        // extension proxy support
        static void staticNotifyPresetLoaded(aap_presets_host_extension_t* ext, AndroidAudioPluginHost* host) {
            ((PresetsServiceAAPXS*) ext->aapxs_context)->notifyPresetLoaded();
        }
        static void staticNotifyPresetsUpdated(aap_presets_host_extension_t* ext, AndroidAudioPluginHost* host) {
            ((PresetsServiceAAPXS*) ext->aapxs_context)->notifyPresetsUpdated();
        }
        aap_presets_host_extension_t as_host_extension{this, staticNotifyPresetLoaded, staticNotifyPresetsUpdated};

    public:
        PresetsServiceAAPXS(AAPXSInitiatorInstance* initiatorInstance, AAPXSSerializationContext* serialization)
                : TypedAAPXS(AAP_PRESETS_EXTENSION_URI, initiatorInstance, serialization) {
        }

        void notifyPresetLoaded();
        void notifyPresetsUpdated();

        aap_presets_host_extension_t* asHostExtension() { return &as_host_extension; }
    };

    class AAPXSDefinition_Presets : public AAPXSDefinitionWrapper {

        static void aapxs_presets_process_incoming_plugin_aapxs_request(
                struct AAPXSDefinition* feature,
                AAPXSRecipientInstance* aapxsInstance,
                AndroidAudioPlugin* plugin,
                AAPXSRequestContext* request);
        static void aapxs_presets_process_incoming_host_aapxs_request(
                struct AAPXSDefinition* feature,
                AAPXSRecipientInstance* aapxsInstance,
                AndroidAudioPluginHost* host,
                AAPXSRequestContext* request);
        static void aapxs_presets_process_incoming_plugin_aapxs_reply(
                struct AAPXSDefinition* feature,
                AAPXSInitiatorInstance* aapxsInstance,
                AndroidAudioPlugin* plugin,
                AAPXSRequestContext* request);
        static void aapxs_presets_process_incoming_host_aapxs_reply(
                struct AAPXSDefinition* feature,
                AAPXSInitiatorInstance* aapxsInstance,
                AndroidAudioPluginHost* host,
                AAPXSRequestContext* request);

        // It is used in synchronous context such as `get_extension()` in `binder-client-as-plugin.cpp` etc.
        static AAPXSExtensionClientProxy aapxs_presets_get_plugin_proxy(
                struct AAPXSDefinition* feature,
                AAPXSInitiatorInstance* aapxsInstance,
                AAPXSSerializationContext* serialization);

        static AAPXSExtensionServiceProxy aapxs_presets_get_host_proxy(
                struct AAPXSDefinition *feature,
                AAPXSInitiatorInstance *aapxsInstance,
                AAPXSSerializationContext *serialization);

        static void* aapxs_presets_as_plugin_extension(AAPXSExtensionClientProxy* proxy) {
            return ((PresetsClientAAPXS*) proxy->aapxs_context)->asPluginExtension();
        }

        static void* aapxs_presets_as_host_extension(AAPXSExtensionServiceProxy* proxy) {
            return ((PresetsServiceAAPXS*) proxy->aapxs_context)->asHostExtension();
        }

        AAPXSDefinition aapxs_presets{this,
                                      AAP_PRESETS_EXTENSION_URI,
                                      PRESETS_SHARED_MEMORY_SIZE,
                                      aapxs_presets_process_incoming_plugin_aapxs_request,
                                      aapxs_presets_process_incoming_host_aapxs_request,
                                      aapxs_presets_process_incoming_plugin_aapxs_reply,
                                      aapxs_presets_process_incoming_host_aapxs_reply,
                                      aapxs_presets_get_plugin_proxy,
                                      aapxs_presets_get_host_proxy
        };

    public:
        AAPXSDefinition& asPublic() override {
            return aapxs_presets;
        }
    };
}

#endif //AAP_CORE_PRESETS_AAPXS_H
