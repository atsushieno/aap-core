
#ifndef AAP_CORE_EXTENSION_SERVICE_H
#define AAP_CORE_EXTENSION_SERVICE_H

#include <vector>
#include "aap/unstable/aapxs.h"
#include "aap/unstable/presets.h"

namespace aap {

class PluginClient;
class LocalPluginInstance;
class RemotePluginInstance;

// FIXME: should this be "AAPXSLocalInstanceWrapper" ?
//  It should be consistent in terms of "remote" vs. "local" instead of "client" vs. "service".
class AAPXSServiceInstanceWrapper {
    LocalPluginInstance* local_plugin_instance;
    AAPXSServiceInstance service;

public:
    AAPXSServiceInstanceWrapper(LocalPluginInstance* pluginInstance, const char* uri, void* shmData, int32_t shmDataSize)
        : local_plugin_instance(pluginInstance) {
        service.uri = uri;
        service.data = shmData;
        service.data_size = shmDataSize;
    }

    LocalPluginInstance* getPluginInstance() { return local_plugin_instance; }
    AAPXSServiceInstance* asPublicApi() { return &service; }
};

// FIXME: should this be "AAPXSRemoteInstanceWrapper" ?
//  It should be consistent in terms of "remote" vs. "local" instead of "client" vs. "service".
class AAPXSClientInstanceWrapper {
    RemotePluginInstance* remote_plugin_instance;
    AAPXSClientInstance client;

public:
    AAPXSClientInstanceWrapper(RemotePluginInstance* pluginInstance, const char* uri, void* shmData, int32_t shmDataSize)
        : remote_plugin_instance(pluginInstance) {
        client.uri = uri;
        client.data = shmData;
        client.data_size = shmDataSize;
    }

    RemotePluginInstance* getPluginInstance() { return remote_plugin_instance; }
    AAPXSClientInstance* asPublicApi() { return &client; }
};

/**
 * wrapper for AAPXSFeature, used by AAP framework hosting API. Not exposed to extension developers.
 */
class AAPXSFeatureWrapper {
    AAPXSFeature feature;

public:
    AAPXSFeatureWrapper(AAPXSFeature feature)
        : feature(feature) {
    }

    inline const char* getUri() { return feature.uri; }

    const AAPXSFeature& data() { return feature; }
};

class PluginExtensionServiceRegistry {
    AAPXSFeatureWrapper empty{AAPXSFeature{"", nullptr, nullptr}};
    std::vector<AAPXSFeatureWrapper> extension_services{};

public:
    inline void add(AAPXSFeatureWrapper extensionService) {
        extension_services.emplace_back(extensionService);
    }

    inline AAPXSFeatureWrapper getByUri(const char * uri) {
        for (auto &e : extension_services)
            if (strcmp(e.getUri(), uri) == 0)
                return e;
        return empty;
    }

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
