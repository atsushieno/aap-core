
#ifndef AAP_CORE_EXTENSION_SERVICE_H
#define AAP_CORE_EXTENSION_SERVICE_H

#include "aap/core/host/audio-plugin-host.h"
#include "aap/unstable/extensions.h"
#include "aap/unstable/presets.h"

namespace aap {

class PluginClient;

/**
 * C++ wrapper for AndroidAudioPluginServiceExtension.
 */
class PluginServiceExtension {
    AndroidAudioPluginExtension proxy;
    AndroidAudioPluginServiceExtension pub;

public:
    PluginServiceExtension(const char *uri, int32_t dataSize, void *data) {
        proxy.uri = uri;
        proxy.transmit_size = dataSize;
        proxy.data = data;

        pub.context = this;
        pub.uri = uri;
        pub.data = &proxy;
    }

    inline AndroidAudioPluginServiceExtension* asTransient() {
        return &pub;
    }
};

class PluginExtensionServiceRegistry {
    std::vector<std::unique_ptr<PluginServiceExtension>> extension_services{};

public:
    inline void add(PluginServiceExtension* extensionService) {
        extension_services.emplace_back(std::move(extensionService));
    }

    PluginServiceExtension* getByUri(const char * uri);

    /*
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
    */
};

} // namespace aap

#endif //AAP_CORE_EXTENSION_SERVICE_H
