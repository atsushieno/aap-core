#ifndef AAP_CORE_PRESETS_AAPXS_H
#define AAP_CORE_PRESETS_AAPXS_H

#include <functional>
#include <future>
#include "aap/unstable/aapxs-vnext.h"
#include "aap/ext/presets.h"

// plugin extension opcodes
const int32_t OPCODE_GET_PRESET_COUNT = 0;
const int32_t OPCODE_GET_PRESET_DATA = 1;
const int32_t OPCODE_GET_PRESET_INDEX = 2;
const int32_t OPCODE_SET_PRESET_INDEX = 3;

// host extension opcodes
const int32_t OPCODE_NOTIFY_PRESET_LOADED = 0;
const int32_t OPCODE_NOTIFY_PRESET_UPDATED = 1;

const int32_t PRESETS_SHARED_MEMORY_SIZE = sizeof(aap_preset_t) + sizeof(int32_t);

namespace aap {
    class AAPXSFeatureImpl {
    protected:
        AAPXSFeatureImpl() {}
    public:
        virtual AAPXSFeatureVNext* asPublic() = 0;

        template<typename T>
        static T correlate(AAPXSRequestContext request, std::function<std::future<T>(std::promise<T> promiseInner)> registerCallback, std::function<T()> getResult) {
            std::promise<T> promiseOuter{};
            std::function<void(std::promise<T>, T)> handleReply = [&](std::promise<T> promise, T result) {
                promise.set_value(getResult());
            };
            std::future<T> f = registerCallback(handleReply, std::move(promiseOuter));
            f.wait();
            return f.get();
        }
    };

    class AAPXSFeaturePresets
            : public AAPXSFeatureImpl {

        static void aapxs_presets_process_incoming_plugin_aapxs_request(
                struct AAPXSFeatureVNext* feature,
                AAPXSRecipientInstance* aapxsInstance,
                AndroidAudioPlugin* plugin,
                AAPXSRequestContext* context);
        static void aapxs_presets_process_incoming_host_aapxs_request(
                struct AAPXSFeatureVNext* feature,
                AAPXSRecipientInstance* aapxsInstance,
                AndroidAudioPluginHost* host,
                AAPXSRequestContext* context);
        static void aapxs_presets_process_incoming_plugin_aapxs_reply(
                struct AAPXSFeatureVNext* feature,
                AAPXSInitiatorInstance* aapxsInstance,
                AndroidAudioPlugin* plugin,
                AAPXSRequestContext* context);
        static void aapxs_presets_process_incoming_host_aapxs_reply(
                struct AAPXSFeatureVNext* feature,
                AAPXSInitiatorInstance* aapxsInstance,
                AndroidAudioPluginHost* host,
                AAPXSRequestContext* context);

        AAPXSFeatureVNext aapxs_presets{AAP_PRESETS_EXTENSION_URI,
                                        nullptr,
                                        PRESETS_SHARED_MEMORY_SIZE,
                                        aapxs_presets_process_incoming_plugin_aapxs_request,
                                        aapxs_presets_process_incoming_host_aapxs_request,
                                        aapxs_presets_process_incoming_plugin_aapxs_reply,
                                        aapxs_presets_process_incoming_host_aapxs_reply
        };

    public:
        AAPXSFeatureVNext * asPublic() override {
            return &aapxs_presets;
        }
    };

    class PresetsClientAAPXS {
        AAPXSInitiatorInstance *initiatorInstance;
        AAPXSSerializationContext *serialization;

        static void getPresetIntCallback(void* callbackContext, AndroidAudioPlugin* plugin, int32_t requestId);
        static void getPresetCallback(void* callbackContext, AndroidAudioPlugin* plugin, int32_t requestId);

    public:
        PresetsClientAAPXS(AAPXSInitiatorInstance* initiatorInstance, AAPXSSerializationContext* serialization)
                : initiatorInstance(initiatorInstance), serialization(serialization) {
        }

        int32_t callIntPresetFunction(int32_t opcode);
        int32_t getPresetCount();
        void getPreset(int32_t index, aap_preset_t& preset);
        int32_t getPresetIndex();
        void setPresetIndex(int32_t index);
    };

    class PresetsServiceAAPXS {
        AAPXSRecipientInstance *recipientInstance;
        AAPXSSerializationContext *serialization;

    public:
        PresetsServiceAAPXS(AAPXSRecipientInstance* instance, AAPXSSerializationContext* serialization)
                : recipientInstance(instance), serialization(serialization) {
        }

        void notifyPresetLoaded();
        void notifyPresetsUpdated();
    };
}

#endif //AAP_CORE_PRESETS_AAPXS_H
