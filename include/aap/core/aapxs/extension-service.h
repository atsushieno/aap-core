
#ifndef AAP_CORE_EXTENSION_SERVICE_H
#define AAP_CORE_EXTENSION_SERVICE_H

#include <string>
#include <vector>
#include <map>
#include <cassert>
#include "aap/unstable/aapxs.h"
#include "aap/unstable/logging.h"
#include "../plugin-information.h"
#include "aap/android-audio-plugin.h"

namespace aap {

template<typename T>
class AAPXSFeatureMap {
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

    inline const char* addOrGetUri(const char* uri) {
        auto i = getUriIndex(uri);
        if (i < 0)
            addUri(uri);
        return getInterned(uri);
    }

    inline void add(const char* uri, std::unique_ptr<T> newInstance) {
        auto interned = addOrGetUri(uri);
        map[getUriIndex(interned, true)] = std::move(newInstance);
    }

    inline T* get(const char* uri, bool onlyInterned = false) {
        auto i = getUriIndex(uri, onlyInterned);
        return i < 0 ? nullptr : map[i].get();
    }
};

template<typename T>
class AAPXSInstanceMap : public AAPXSFeatureMap<T> {
    std::function<AndroidAudioPlugin*()> get_plugin;
public:
    AAPXSInstanceMap(std::function<AndroidAudioPlugin*()> getPluginFunc)
            : get_plugin(getPluginFunc) {
    }

    AndroidAudioPlugin *getPlugin() { return get_plugin(); }
};

class AAPXSRegistry {
    std::vector<std::unique_ptr<const char>> stringpool{};
    AAPXSFeatureMap<AAPXSFeature> extension_services{};
    bool readonly{false};

public:
    inline void freeze() { readonly = true; }

    inline void add(AAPXSFeature* extensionService) {
        assert(!readonly);
        if (readonly)
            return;
        auto e = std::make_unique<AAPXSFeature>();
        auto f = extensionService;
        e->context = f->context;
        e->is_implementation_mandatory = f->is_implementation_mandatory;
        e->shared_memory_size = f->shared_memory_size;
        e->as_proxy = f->as_proxy;
        e->on_invoked = f->on_invoked;
        extension_services.add(f->uri, std::move(e));
        // It is somewhat ugly, but we have to replace uri field with interned one here.
        extension_services.get(f->uri)->uri = extension_services.addOrGetUri(f->uri);
    }

    inline AAPXSFeature* getByUri(const char * uri) {
        return (AAPXSFeature*) extension_services.get(uri);
    }
};

/**
 * This class aims to isolate AAPXS client instance management job from native hosting
 * implementation such as RemotePluginInstance.
 * For other implementations (e.g. we also have Kotlin hosting API), we still want to manage
 * AAPXS, and then we need something that is independent of the native hosting implementation, like this.
 */
class AAPXSClientInstanceManager {
protected:
    AAPXSFeatureMap<AAPXSClientInstance> aapxsClientInstances{};

public:
    virtual ~AAPXSClientInstanceManager() {}

    virtual AndroidAudioPlugin* getPlugin() = 0;
    virtual AAPXSFeature* getExtensionFeature(const char* uri) = 0;
    virtual const PluginInformation* getPluginInformation() = 0;
    virtual AAPXSClientInstance* setupAAPXSInstance(AAPXSFeature *feature, int32_t dataSize = -1) = 0;

    AAPXSClientInstance* getInstanceFor(const char* uri) {
        return aapxsClientInstances.get(uri);
    }

    /**
     * For host developers, it is the only entry point to get extension.
     * The return value is AAPXSProxyContext which contains the (strongly typed) extension proxy value.
     */
    AAPXSProxyContext getExtensionProxy(const char* uri);

    /**
     * It is invoked by AAP framework (actually binder-client-as-plugin) to set up AAPXS client instance
     * for each supported extension, while leaving this function to determine what extensions to provide.
     * It is called at completeInstantiation() step, for each plugin instance.
     *
     * Returns false if any error occured.
     */
    [[nodiscard]] bool setupAAPXSInstances(std::function<void(AAPXSClientInstance*)> func);
};

} // namespace aap

#endif //AAP_CORE_EXTENSION_SERVICE_H
