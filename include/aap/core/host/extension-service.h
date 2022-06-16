
#ifndef AAP_CORE_EXTENSION_SERVICE_H
#define AAP_CORE_EXTENSION_SERVICE_H

#include <string>
#include <vector>
#include <map>
#include <cassert>
#include "aap/unstable/aapxs.h"
#include "aap/unstable/presets.h"
#include "aap/unstable/logging.h"
#include "plugin-information.h"

namespace aap {

class PluginClient;
class LocalPluginInstance;
class RemotePluginInstance;

template<typename T>
class AAPXSInstanceMap {
    std::vector<std::unique_ptr<const char>> uris{};
    std::map<size_t, std::unique_ptr<T>> map;

    inline int32_t addUri(const char* uri) {
        assert(getUriIndex(uri) < 0);
        uris.emplace_back(strdup(uri));
        return (int32_t) uris.size() - 1;
    }

public:
    // LV2 URID alike: the interned pointer can be used for quick lookup.
    // Use it with `onlyInterned = true` in getUriIndex() and get().
    inline const char* getInterned(const char* uri) {
        for (size_t i = 0, n = uris.size(); i < n; i++)
            if (strcmp(uris[i].get(), uri) == 0)
                return uris[i].get();
        return nullptr;
    }

    inline int32_t getUriIndex(const char* uri, bool onlyInterned = false) {
        if (uri == nullptr)
            return -1;
        for (size_t i = 0, n = uris.size(); i < n; i++)
            if (uris[i].get() == uri)
                return i;
        if (onlyInterned)
            return -1;
        for (size_t i = 0, n = uris.size(); i < n; i++)
            if (strcmp(uris[i].get(), uri) == 0)
                return i;
        return -1;
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
    std::unique_ptr<const char> uri;  // argument extensionUri is not persistent, returned by std::string.c_str(). We need another persistent-ish one.
    RemotePluginInstance* remote_plugin_instance;
    AAPXSClientInstance client{};

public:
    AAPXSClientInstanceWrapper(RemotePluginInstance* pluginInstance, const char* extensionUri, void* shmData, int32_t shmDataSize);

    RemotePluginInstance* getPluginInstance() { return remote_plugin_instance; }
    AAPXSClientInstance* asPublicApi() { return &client; }
};

class AAPXSRegistry {
    std::vector<std::unique_ptr<const char>> stringpool{};
    AAPXSInstanceMap<AAPXSFeature> extension_services{};

public:
    inline void add(AAPXSFeature* extensionService) {
        auto e = std::make_unique<AAPXSFeature>();
        auto f = extensionService;
        e->context = f->context;
        e->shared_memory_size = f->shared_memory_size;
        e->as_proxy = f->as_proxy;
        e->on_invoked = f->on_invoked;
        extension_services.add(f->uri, std::move(e));
        // It is somewhat ugly, but we have to replace uri field with interned one here.
        extension_services.get(f->uri)->uri = extension_services.getInterned(f->uri);
    }

    inline AAPXSFeature* getByUri(const char * uri) {
        return (AAPXSFeature*) extension_services.get(uri);
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

/**
 * This class aims to isolate AAPXS client instance management job from RemotePluginInstance.
 *
 * RemotePluginInstance is a native hosting feature, and hence core of libandroidaudioplugin
 * hosting implementation.
 * For other implementations (e.g. we also have Kotlin hosting API), we still want to manage
 * AAPXS, and then we need something that is independent of the native hosting implementation.
 *
 * Though it is not achieved yet, because AAPXSClientInstanceWrapper depends on
 * RemotePluginInstance. We need further isolation.
 */
class AAPXSClientInstanceManager {
protected:
    AAPXSInstanceMap<AAPXSClientInstanceWrapper> aapxsClientInstanceWrappers{};

public:
    virtual ~AAPXSClientInstanceManager() {}

    aapxs_client_extension_message_t static_send_extension_message_func{nullptr};

    virtual AndroidAudioPlugin* getPlugin() = 0;
    virtual AAPXSFeature* getExtensionFeature(const char* uri) = 0;
    virtual const PluginInformation* getPluginInformation() = 0;
    virtual AAPXSClientInstanceWrapper* setupAAPXSInstanceWrapper(AAPXSFeature *feature, int32_t dataSize = -1) = 0;

    AAPXSClientInstanceWrapper* getAAPXSWrapper(const char* uri) {
        auto ret = aapxsClientInstanceWrappers.get(uri);
        assert(ret);
        return ret;
    }

    // For host developers, it is the only entry point to get extension.
    // The return value is AAPXSProxyContext which contains the (strongly typed) extension proxy value.
    AAPXSProxyContext getExtensionProxy(const char* uri);

    // It is invoked by AAP framework (actually binder-client-as-plugin) to set up AAPXS client instance
    // for each supported extension, while leaving this function to determine what extensions to provide.
    // It is called at completeInstantiation() step, for each plugin instance.
    void setupAAPXSInstances(std::function<void(AAPXSClientInstance*)> func);
};

} // namespace aap

#endif //AAP_CORE_EXTENSION_SERVICE_H
