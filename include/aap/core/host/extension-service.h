
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
#include "aap/android-audio-plugin.h"
#include "aap/unstable/presets.h"
#include "aap/unstable/state.h"

namespace aap {

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
};

/**
 * This class aims to isolate AAPXS client instance management job from native hosting
 * implementation such as RemotePluginInstance.
 * For other implementations (e.g. we also have Kotlin hosting API), we still want to manage
 * AAPXS, and then we need something that is independent of the native hosting implementation.
 */
class AAPXSClientInstanceManager {
protected:
    AAPXSInstanceMap<AAPXSClientInstance> aapxsClientInstances{};

public:
    virtual ~AAPXSClientInstanceManager() {}

    aapxs_client_extension_message_t static_send_extension_message_func{nullptr};

    virtual AndroidAudioPlugin* getPlugin() = 0;
    virtual AAPXSFeature* getExtensionFeature(const char* uri) = 0;
    virtual const PluginInformation* getPluginInformation() = 0;
    virtual AAPXSClientInstance* setupAAPXSInstance(AAPXSFeature *feature, int32_t dataSize = -1) = 0;

    AAPXSClientInstance* getInstanceFor(const char* uri) {
        auto ret = aapxsClientInstances.get(uri);
        assert(ret);
        return ret;
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
     */
    void setupAAPXSInstances(std::function<void(AAPXSClientInstance*)> func);
};

//----

class StandardExtensions {

    // Since we cannot define template function as virtual, they are defined using macros instead...
#define DEFINE_WITH_STATE_EXTENSION(TYPE) \
virtual TYPE withStateExtension(TYPE defaultValue, std::function<TYPE(aap_state_extension_t*, AndroidAudioPluginExtensionTarget target)> func) = 0;

    DEFINE_WITH_STATE_EXTENSION(size_t)
    DEFINE_WITH_STATE_EXTENSION(const aap_state_t&)
    virtual void withStateExtension(std::function<void(aap_state_extension_t*, AndroidAudioPluginExtensionTarget target)> func) = 0;

#define DEFINE_WITH_PRESETS_EXTENSION(TYPE) \
virtual TYPE withPresetsExtension(TYPE defaultValue, std::function<TYPE(aap_presets_extension_t*, AndroidAudioPluginExtensionTarget)> func) = 0;

    DEFINE_WITH_PRESETS_EXTENSION(int32_t)
    DEFINE_WITH_PRESETS_EXTENSION(std::string)
    virtual void withPresetsExtension(std::function<void(aap_presets_extension_t*, AndroidAudioPluginExtensionTarget)> func) = 0;

    aap_state_t dummy_state{nullptr, 0}, state{nullptr, 0};

#define DEFAULT_STATE_BUFFER_SIZE 65536

    void ensureStateBuffer(size_t sizeInBytes) {
        if (state.data == nullptr) {
            state.data = calloc(1, DEFAULT_STATE_BUFFER_SIZE);
            state.data_size = DEFAULT_STATE_BUFFER_SIZE;
        } else if (state.data_size >= sizeInBytes) {
            free(state.data);
            state.data = calloc(1, state.data_size * 2);
            state.data_size *= 2;
        }
    }

public:
    size_t getStateSize() {
        return withStateExtension/*<size_t>*/(0, [&](aap_state_extension_t* ext, AndroidAudioPluginExtensionTarget target) {
            return ext->get_state_size(target);
        });
    }

    const aap_state_t& getState() {
        return withStateExtension/*<const aap_state_t&>*/(dummy_state, [&](aap_state_extension_t* ext, AndroidAudioPluginExtensionTarget target) {
            ext->get_state(target, &state);
            return state;
        });
    }

    void setState(const void* data, size_t sizeInBytes) {
        withStateExtension/*<int>*/(0, [&](aap_state_extension_t* ext, AndroidAudioPluginExtensionTarget target) {
            ensureStateBuffer(sizeInBytes);
            memcpy(state.data, data, sizeInBytes);
            ext->set_state(target, &state);
            return 0;
        });
    }

    int32_t getPresetCount()
    {
        return withPresetsExtension/*<int32_t>*/(0, [&](aap_presets_extension_t* ext, AndroidAudioPluginExtensionTarget target) {
            return ext->get_preset_count(target);
        });
    }

    int32_t getCurrentPresetIndex()
    {
        return withPresetsExtension/*<int32_t>*/(0, [&](aap_presets_extension_t* ext, AndroidAudioPluginExtensionTarget target) {
            return ext->get_preset_index(target);
        });
    }

    void setCurrentPresetIndex(int index)
    {
        withPresetsExtension/*<int32_t>*/(0, [&](aap_presets_extension_t* ext, AndroidAudioPluginExtensionTarget target) {
            ext->set_preset_index(target, index);
            return 0;
        });
    }

    std::string getCurrentPresetName(int index)
    {
        return withPresetsExtension/*<std::string>*/("", [&](aap_presets_extension_t* ext, AndroidAudioPluginExtensionTarget target) {
            aap_preset_t result;
            ext->get_preset(target, index, true, &result);
            return std::string{result.name};
        });
    }
};

class LocalPluginInstanceStandardExtensions : public StandardExtensions {
    template<typename T, typename X> T withExtension(T defaultValue, const char* extensionUri, std::function<T(X*, AndroidAudioPluginExtensionTarget target)> func) {
        auto plugin = getPlugin();
        auto ext = (X*) plugin->get_extension(plugin, extensionUri);
        if (ext == nullptr)
            return defaultValue;
        return func(ext, AndroidAudioPluginExtensionTarget{plugin, nullptr});
    }

#define DEFINE_WITH_STATE_EXTENSION_LOCAL(TYPE) \
TYPE withStateExtension(TYPE defaultValue, std::function<TYPE(aap_state_extension_t*, AndroidAudioPluginExtensionTarget target)> func) override { return withExtension<TYPE, aap_state_extension_t>(defaultValue, AAP_STATE_EXTENSION_URI, func); }

    DEFINE_WITH_STATE_EXTENSION_LOCAL(size_t)
    DEFINE_WITH_STATE_EXTENSION_LOCAL(const aap_state_t&)
    void withStateExtension(std::function<void(aap_state_extension_t*, AndroidAudioPluginExtensionTarget target)> func) override {
        withExtension<int, aap_state_extension_t>(0, AAP_STATE_EXTENSION_URI, [&](aap_state_extension_t* ext, AndroidAudioPluginExtensionTarget target) {
            func(ext, target);
            return 0;
        });
    }

#define DEFINE_WITH_PRESETS_EXTENSION_LOCAL(TYPE) \
TYPE withPresetsExtension(TYPE defaultValue, std::function<TYPE(aap_presets_extension_t*, AndroidAudioPluginExtensionTarget target)> func) override { return withExtension<TYPE, aap_presets_extension_t>(defaultValue, AAP_PRESETS_EXTENSION_URI, func); }

    DEFINE_WITH_PRESETS_EXTENSION_LOCAL(int32_t)
    DEFINE_WITH_PRESETS_EXTENSION_LOCAL(std::string)
    virtual void withPresetsExtension(std::function<void(aap_presets_extension_t*, AndroidAudioPluginExtensionTarget)> func) override {
        withExtension<int, aap_presets_extension_t>(0, AAP_STATE_EXTENSION_URI, [&](aap_presets_extension_t* ext, AndroidAudioPluginExtensionTarget target) {
            func(ext, target);
            return 0;
        });
    }

public:
    virtual AndroidAudioPlugin* getPlugin() = 0;
};

class RemotePluginInstanceStandardExtensions : public StandardExtensions {
    template<typename T, typename X> T withExtension(T defaultValue, const char* extensionUri, std::function<T(X*, AndroidAudioPluginExtensionTarget target)> func) {
        auto proxyContext = getAAPXSManager()->getExtensionProxy(extensionUri);
        auto ext = (X*) proxyContext.extension;
        if (ext == nullptr)
            return defaultValue;
        AndroidAudioPluginExtensionTarget target;
        target.aapxs_context = proxyContext.aapxs_context; // it should be PresetsPluginClientExtension::Instance
        target.plugin = getPlugin();
        return func(ext, target);
    }

#define DEFINE_WITH_STATE_EXTENSION_REMOTE(TYPE) \
TYPE withStateExtension(TYPE defaultValue, std::function<TYPE(aap_state_extension_t*, AndroidAudioPluginExtensionTarget target)> func) override { return withExtension<TYPE, aap_state_extension_t>(defaultValue, AAP_STATE_EXTENSION_URI, func); }

    DEFINE_WITH_STATE_EXTENSION_REMOTE(size_t)
    DEFINE_WITH_STATE_EXTENSION_REMOTE(const aap_state_t&)
    void withStateExtension(std::function<void(aap_state_extension_t*, AndroidAudioPluginExtensionTarget target)> func) override {
        withExtension<int, aap_state_extension_t>(0, AAP_STATE_EXTENSION_URI, [&](aap_state_extension_t* ext, AndroidAudioPluginExtensionTarget target) {
            func(ext, target);
            return 0;
        });
    }

#define DEFINE_WITH_PRESETS_EXTENSION_REMOTE(TYPE) \
TYPE withPresetsExtension(TYPE defaultValue, std::function<TYPE(aap_presets_extension_t*, AndroidAudioPluginExtensionTarget target)> func) override { return withExtension<TYPE, aap_presets_extension_t>(defaultValue, AAP_PRESETS_EXTENSION_URI, func); }

    DEFINE_WITH_PRESETS_EXTENSION_REMOTE(int32_t)
    DEFINE_WITH_PRESETS_EXTENSION_REMOTE(std::string)
    virtual void withPresetsExtension(std::function<void(aap_presets_extension_t*, AndroidAudioPluginExtensionTarget)> func) override {
        withExtension<int, aap_presets_extension_t>(0, AAP_STATE_EXTENSION_URI, [&](aap_presets_extension_t* ext, AndroidAudioPluginExtensionTarget target) {
            func(ext, target);
            return 0;
        });
    }

public:
    virtual AndroidAudioPlugin* getPlugin() = 0;
    virtual AAPXSClientInstanceManager* getAAPXSManager() = 0;
};


} // namespace aap

#endif //AAP_CORE_EXTENSION_SERVICE_H
