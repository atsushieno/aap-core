
#ifndef AAP_CORE_EXTENSION_SERVICE_H
#define AAP_CORE_EXTENSION_SERVICE_H

#include <string>
#include <vector>
#include <map>
#include "aap/unstable/aapxs.h"
#include "aap/unstable/presets.h"
#include "aap/unstable/logging.h"

namespace aap {

class PluginClient;
class LocalPluginInstance;
class RemotePluginInstance;

// FIXME: should this be "AAPXSLocalInstanceWrapper" ?
//  It should be consistent in terms of "remote" vs. "local" instead of "client" vs. "service".
class AAPXSServiceInstanceWrapper {
    std::string uri;
    LocalPluginInstance* local_plugin_instance;
    AAPXSServiceInstance service;

public:
    AAPXSServiceInstanceWrapper(LocalPluginInstance* pluginInstance, const char* extensionUri, void* shmData, int32_t shmDataSize)
        : local_plugin_instance(pluginInstance) {
        uri = extensionUri;
        service.uri = uri.c_str();
        service.data = shmData;
        service.data_size = shmDataSize;
    }

    LocalPluginInstance* getPluginInstance() { return local_plugin_instance; }
    AAPXSServiceInstance* asPublicApi() { return &service; }
};

// FIXME: should this be "AAPXSRemoteInstanceWrapper" ?
//  It should be consistent in terms of "remote" vs. "local" instead of "client" vs. "service".
class AAPXSClientInstanceWrapper {
    std::string uri;  // argument extensionUri is not persistent, returned by std::string.c_str(). We need another persistent-ish one.
    RemotePluginInstance* remote_plugin_instance;
    AAPXSClientInstance client{};

public:
    AAPXSClientInstanceWrapper(RemotePluginInstance* pluginInstance, const char* extensionUri, void* shmData, int32_t shmDataSize);

    RemotePluginInstance* getPluginInstance() { return remote_plugin_instance; }
    AAPXSClientInstance* asPublicApi() { return &client; }
};

/**
 * wrapper for AAPXSFeature, used by AAP framework hosting API. Not exposed to extension developers.
 */
class AAPXSFeatureWrapper {
    std::string uri;
    AAPXSFeature feature;

public:
    AAPXSFeatureWrapper(AAPXSFeature sourceFeature)
        : feature(sourceFeature) {
        uri = sourceFeature.uri;
        feature.uri = uri.data();
    }

    inline const char* getUri() { return uri.c_str(); }

    const AAPXSFeature& data() { return feature; }
};

class AAPXSRegistry {
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
    class StandardAAPXSRegistry {
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

template<typename T>
class AAPXSInstanceMap {
    std::vector<std::string> stringpool{};
    std::vector<const char*> uris{};
    std::map<size_t, std::unique_ptr<T>> map;

public:
    // LV2 URID alike: the interned pointer can be used for quick lookup.
    // Use it with `onlyInterned = true` in getUriIndex() and get().
    inline const char* getInterned(const char* uri) {
        for (size_t i = 0, n = uris.size(); i < n; i++)
            if (strcmp(uris[i], uri) == 0)
                return uris[i];
        return nullptr;
    }

    inline int32_t getUriIndex(const char* uri, bool onlyInterned = false) {
        if (uri == nullptr)
            return -1;
        for (size_t i = 0, n = uris.size(); i < n; i++)
            if (uris[i] == uri)
                return i;
        if (onlyInterned)
            return -1;
        for (size_t i = 0, n = uris.size(); i < n; i++)
            if (strcmp(uris[i], uri) == 0)
                return i;
        return -1;
    }

    inline int32_t addUri(const char* uri) {
        auto interned = getInterned(uri);
        assert(getUriIndex(interned) < 0);
        stringpool.emplace_back(uri);
        uris.emplace_back(stringpool[stringpool.size() - 1].c_str());
        return uris.size() - 1;
    }

    inline int32_t addOrGetUri(const char* uri) {
        auto i = getUriIndex(uri);
        return i >= 0 ? i : addUri(uri);
    }

    inline void add(const char* uri, std::unique_ptr<T> newInstance) {
        assert(getUriIndex(uri) < 0);
        map[addUri(uri)] = std::move(newInstance);
    }

    inline T* get(const char* uri, bool onlyInterned = false) {
        auto i = getUriIndex(uri, onlyInterned);
        return i < 0 ? nullptr : map[i].get();
    }
};

} // namespace aap

#endif //AAP_CORE_EXTENSION_SERVICE_H
