#ifndef AAP_CORE_PRESETS_AAPXS_H
#define AAP_CORE_PRESETS_AAPXS_H

#include <functional>
#include <future>
#include "../../unstable/aapxs-vnext.h"
#include "../../ext/presets.h"
#include "aapxs-runtime.h"

// plugin extension opcodes
const int32_t OPCODE_GET_PRESET_COUNT = 0;
const int32_t OPCODE_GET_PRESET_DATA = 1;
const int32_t OPCODE_GET_PRESET_INDEX = 2;
const int32_t OPCODE_SET_PRESET_INDEX = 3;

// host extension opcodes
const int32_t OPCODE_NOTIFY_PRESET_LOADED = -1;
const int32_t OPCODE_NOTIFY_PRESETS_UPDATED = -2;

const int32_t PRESETS_SHARED_MEMORY_SIZE = sizeof(aap_preset_t) + sizeof(int32_t);

namespace aap::xs {
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

        AAPXSDefinition aapxs_presets{AAP_PRESETS_EXTENSION_URI,
                                      this,
                                      PRESETS_SHARED_MEMORY_SIZE,
                                      aapxs_presets_process_incoming_plugin_aapxs_request,
                                      aapxs_presets_process_incoming_host_aapxs_request,
                                      aapxs_presets_process_incoming_plugin_aapxs_reply,
                                      aapxs_presets_process_incoming_host_aapxs_reply
        };

    public:
        AAPXSDefinition& asPublic() override {
            return aapxs_presets;
        }
    };

    class PresetsClientAAPXS : public TypedAAPXS {
    public:
        PresetsClientAAPXS(AAPXSInitiatorInstance* initiatorInstance, AAPXSSerializationContext* serialization)
                : TypedAAPXS(AAP_PRESETS_EXTENSION_URI, initiatorInstance, serialization) {
        }

        int32_t getPresetCount();
        void getPreset(int32_t index, aap_preset_t& preset);
        int32_t getPresetIndex();
        void setPresetIndex(int32_t index);
    };

    class PresetsServiceAAPXS : public TypedAAPXS {
    public:
        PresetsServiceAAPXS(AAPXSInitiatorInstance* initiatorInstance, AAPXSSerializationContext* serialization)
                : TypedAAPXS(AAP_PRESETS_EXTENSION_URI, initiatorInstance, serialization) {
        }

        void notifyPresetLoaded();
        void notifyPresetsUpdated();
    };
}

#endif //AAP_CORE_PRESETS_AAPXS_H
