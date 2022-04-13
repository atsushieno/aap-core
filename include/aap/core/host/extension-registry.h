
#ifndef AAP_CORE_EXTENSION_REGISTRY_H
#define AAP_CORE_EXTENSION_REGISTRY_H

#include "aap/core/host/audio-plugin-host.h"
#include "aap/unstable/extensions.h"
#include "aap/unstable/presets.h"

namespace aap {

class PluginExtensionServiceRegistry {
    std::vector<AndroidAudioPluginServiceExtensionFactory*> factories;

public:
    inline void registerFactory(AndroidAudioPluginServiceExtensionFactory* factory) {
        factories.emplace_back(factory);
    }

    AndroidAudioPluginServiceExtension* create(const char * uri, AndroidAudioPluginHost* host, AndroidAudioPluginExtension *extensionInstance);

    class StandardExtensions {
        aap_presets_context_t *preset_context{nullptr};
    public:
        int32_t getPresetCount(PluginClient *client, int32_t instanceId) {
            auto instance = client->getInstance(instanceId);
            auto extension = (aap_presets_extension_t *) instance->getExtension(AAP_PRESETS_EXTENSION_URI);
            return extension->get_preset_count(preset_context);
        }

        int32_t getPresetDataSize(PluginClient *client, int32_t instanceId, int32_t index) {
            auto instance = client->getInstance(instanceId);
            auto extension = (aap_presets_extension_t *) instance->getExtension(AAP_PRESETS_EXTENSION_URI);
            return extension->get_preset_data_size(preset_context, index);
        }

        void getPreset(PluginClient *client, int32_t instanceId, int32_t index, bool skipBinary, aap_preset_t *preset) {
            auto instance = client->getInstance(instanceId);
            auto extension = (aap_presets_extension_t*) instance->getExtension(AAP_PRESETS_EXTENSION_URI);
            extension->get_preset(preset_context, index, skipBinary, preset);
        }
    };
};

} // namespace aap

#endif //AAP_CORE_EXTENSION_REGISTRY_H
