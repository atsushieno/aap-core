#ifndef AAP_CORE_STANDARD_EXTENSIONS_H
#define AAP_CORE_STANDARD_EXTENSIONS_H

#include <future>
#include "extension-service.h"
#include "aap/ext/parameters.h"
#include "aap/ext/midi.h"
#include "aap/ext/state.h"
#include "aap/ext/presets.h"
#include "aap/ext/gui.h"

namespace aap {

class StandardExtensions {
    virtual bool supportsAsynchronousCalls() = 0;

    // Since we cannot define template function as virtual, they are defined using macros instead...
#define AAP_INTERNAL_DEFINE_WITH_STATE_EXTENSION(TYPE) \
virtual TYPE withStateExtension(TYPE defaultValue, std::function<TYPE(aap_state_extension_t*, AndroidAudioPlugin *target)> func) = 0;

    AAP_INTERNAL_DEFINE_WITH_STATE_EXTENSION(size_t)
    AAP_INTERNAL_DEFINE_WITH_STATE_EXTENSION(const aap_state_t&)
    virtual void withStateExtension(std::function<void(aap_state_extension_t*, AndroidAudioPlugin *target)> func) = 0;

#define AAP_INTERNAL_DEFINE_WITH_PRESETS_EXTENSION_ASYNC(TYPE) \
virtual void withPresetsExtensionAsync(std::promise<TYPE> promise, TYPE defaultValue, std::function<void(aap_presets_extension_t*, AndroidAudioPlugin*, std::promise<TYPE>)>) = 0;
#define AAP_INTERNAL_DEFINE_WITH_PRESETS_EXTENSION(TYPE) \
virtual TYPE withPresetsExtension(TYPE defaultValue, std::function<TYPE(aap_presets_extension_t*, AndroidAudioPlugin*)> func) = 0;

    AAP_INTERNAL_DEFINE_WITH_PRESETS_EXTENSION(int32_t)
    AAP_INTERNAL_DEFINE_WITH_PRESETS_EXTENSION(std::string)
    AAP_INTERNAL_DEFINE_WITH_PRESETS_EXTENSION_ASYNC(std::string)
    virtual void withPresetsExtension(std::function<void(aap_presets_extension_t*, AndroidAudioPlugin*)> func) = 0;

    aap_state_t dummy_state{nullptr, 0}, state{nullptr, 0};

#define AAP_INTERNAL_DEFAULT_STATE_BUFFER_SIZE 65536

    void ensureStateBuffer(size_t sizeInBytes) {
        if (state.data == nullptr) {
            state.data = calloc(1, AAP_INTERNAL_DEFAULT_STATE_BUFFER_SIZE);
            state.data_size = AAP_INTERNAL_DEFAULT_STATE_BUFFER_SIZE;
        } else if (state.data_size >= sizeInBytes) {
            free(state.data);
            state.data = calloc(1, state.data_size * 2);
            state.data_size *= 2;
        }
    }

#define AAP_INTERNAL_DEFINE_WITH_PARAMETERS_EXTENSION(TYPE) \
virtual TYPE withParametersExtension(TYPE dummyValue, std::function<TYPE(aap_parameters_extension_t*, AndroidAudioPlugin*)> func) = 0;

    AAP_INTERNAL_DEFINE_WITH_PARAMETERS_EXTENSION(int32_t)
    virtual void withParametersExtension(std::function<void(aap_parameters_extension_t*, AndroidAudioPlugin*)> func) = 0;

#define AAP_INTERNAL_DEFINE_WITH_MIDI_EXTENSION(TYPE) \
virtual TYPE withMidiExtension(TYPE dummyValue, std::function<TYPE(aap_midi_extension_t*, AndroidAudioPlugin*)> func) = 0;

    AAP_INTERNAL_DEFINE_WITH_MIDI_EXTENSION(int32_t)
    virtual void withMidiExtension(std::function<void(aap_midi_extension_t*, AndroidAudioPlugin*)> func) = 0;

#define AAP_INTERNAL_DEFINE_WITH_GUI_EXTENSION(TYPE) \
virtual TYPE withGuiExtension(TYPE dummyValue, std::function<TYPE(aap_gui_extension_t*, AndroidAudioPlugin* plugin)> func) = 0;

    AAP_INTERNAL_DEFINE_WITH_GUI_EXTENSION(int32_t)
    virtual void withGuiExtension(std::function<void(aap_gui_extension_t*, AndroidAudioPlugin*)> func) = 0;

public:
    size_t getStateSize() {
        return withStateExtension/*<size_t>*/(0, [&](aap_state_extension_t* ext, AndroidAudioPlugin* plugin) {
            return ext->get_state_size(ext, plugin);
        });
    }

    const aap_state_t& getState() {
        return withStateExtension/*<const aap_state_t&>*/(dummy_state, [&](aap_state_extension_t* ext, AndroidAudioPlugin* plugin) {
            ext->get_state(ext, plugin, &state);
            return state;
        });
    }

    void setState(const void* data, size_t sizeInBytes) {
        withStateExtension/*<int>*/(0, [&](aap_state_extension_t* ext, AndroidAudioPlugin* plugin) {
            ensureStateBuffer(sizeInBytes);
            memcpy(state.data, data, sizeInBytes);
            ext->set_state(ext, plugin, &state);
            return 0;
        });
    }

    int32_t getPresetCount()
    {
        return withPresetsExtension/*<int32_t>*/(0, [&](aap_presets_extension_t* ext, AndroidAudioPlugin* plugin) {
            return ext->get_preset_count(ext, plugin);
        });
    }

    int32_t getCurrentPresetIndex()
    {
        return withPresetsExtension/*<int32_t>*/(0, [&](aap_presets_extension_t* ext, AndroidAudioPlugin* plugin) {
            return ext->get_preset_index(ext, plugin);
        });
    }

    void setCurrentPresetIndex(int index)
    {
        withPresetsExtension/*<int32_t>*/(0, [&](aap_presets_extension_t* ext, AndroidAudioPlugin* plugin) {
            ext->set_preset_index(ext, plugin, index);
            return 0;
        });
    }

    struct PromiseWrapper {
        aap_preset_t* preset;
        std::promise<std::string> promise;
    };

    static void getPresetCallback(void* context, void* pluginOrHost, int32_t requestId) {
        auto pw = (PromiseWrapper*) context;
        pw->promise.set_value(pw->preset->name);
    }

    std::string getPresetName(int index)
    {
        if (supportsAsynchronousCalls()) {
            auto pw = new PromiseWrapper();
            std::promise<std::string> p;
            auto future = p.get_future();
            withPresetsExtensionAsync(std::move(p), "", [&](aap_presets_extension_t* ext, AndroidAudioPlugin* plugin, std::promise<std::string> promise) {
                aap_preset_t result;
                pw->preset = &result;
                pw->promise = std::move(promise);
                ext->get_preset(ext, plugin, index, &result, getPresetCallback, pw);
            });
            future.wait();
            auto ret = future.get();
            delete pw;
            return ret;
        } else {
            return withPresetsExtension("", [&](aap_presets_extension_t* ext, AndroidAudioPlugin* plugin) {
                aap_preset_t result;
                ext->get_preset(ext, plugin, index, &result, nullptr, nullptr);
                return std::string{result.name};
            });
        }
    }

    int32_t getMidiMappingPolicy() {
        return withMidiExtension(0, [&](aap_midi_extension_t* ext, AndroidAudioPlugin* plugin) {
            if (ext && ext->get_mapping_policy)
                return (int32_t) ext->get_mapping_policy(ext, plugin);
            else
                return (int32_t) AAP_PARAMETERS_MAPPING_POLICY_SYSEX8;
        });
    }

    aap_gui_instance_id createGui(std::string pluginId, int32_t instanceId, void* audioPluginView) {
        return withGuiExtension(0, [&](aap_gui_extension_t* ext, AndroidAudioPlugin* plugin) {
            if (!ext)
                return AAP_GUI_ERROR_NO_GUI_DEFINED;
            if (!ext->create)
                return AAP_GUI_ERROR_NO_CREATE_DEFINED;
            return ext->create(ext, plugin, pluginId.c_str(), instanceId, audioPluginView);
        });
    }

    int32_t showGui(aap_gui_instance_id guiInstanceId) {
        return withGuiExtension(0, [&](aap_gui_extension_t* ext, AndroidAudioPlugin* plugin) {
            if (!ext)
                return AAP_GUI_ERROR_NO_GUI_DEFINED;
            if (!ext->show)
                return AAP_GUI_ERROR_NO_SHOW_DEFINED;
            return ext->show(ext, plugin, guiInstanceId);
        });
    }

    int32_t hideGui(aap_gui_instance_id guiInstanceId) {
        return withGuiExtension(0, [&](aap_gui_extension_t* ext, AndroidAudioPlugin* plugin) {
            if (!ext)
                return AAP_GUI_ERROR_NO_GUI_DEFINED;
            if (!ext->hide)
                return AAP_GUI_ERROR_NO_HIDE_DEFINED;
            ext->hide(ext, plugin, guiInstanceId);
            return AAP_GUI_RESULT_OK;
        });
    }

    int32_t resizeGui(aap_gui_instance_id guiInstanceId, int32_t width, int32_t height) {
        return withGuiExtension(0, [&](aap_gui_extension_t* ext, AndroidAudioPlugin* plugin) {
            if (!ext)
                return AAP_GUI_ERROR_NO_GUI_DEFINED;
            if (!ext->resize)
                return AAP_GUI_ERROR_NO_RESIZE_DEFINED;
            return ext->resize(ext, plugin, guiInstanceId, width,height);
        });
    }

    int32_t destroyGui(aap_gui_instance_id guiInstanceId) {
        return withGuiExtension(0, [&](aap_gui_extension_t* ext, AndroidAudioPlugin* plugin) {
            if (!ext)
                return AAP_GUI_ERROR_NO_GUI_DEFINED;
            if (!ext->destroy)
                return AAP_GUI_ERROR_NO_DESTROY_DEFINED;
            ext->destroy(ext, plugin, guiInstanceId);
            return AAP_GUI_RESULT_OK;
        });
    }
};

class LocalPluginInstanceStandardExtensions : public StandardExtensions {
    bool supportsAsynchronousCalls() override { return false; }

    template<typename T, typename X> T withPluginExtension(bool mandatory, T defaultValue, const char* extensionUri, std::function<T(X*, AndroidAudioPlugin* plugin)> func) {
        auto plugin = getPlugin();
        auto ext = (X*) plugin->get_extension(plugin, extensionUri);
        if (ext == nullptr && mandatory)
            return defaultValue;
        return func(ext, plugin);
    }
    template<typename T, typename X> void withPluginExtensionAsync(bool mandatory, std::promise<T> promise, T defaultValue, const char* extensionUri, std::function<void(X*, AndroidAudioPlugin* plugin, std::promise<T>)> func) {
        auto plugin = getPlugin();
        auto ext = (X*) plugin->get_extension(plugin, extensionUri);
        if (ext == nullptr && mandatory)
            promise.set_value(defaultValue);
        else
            func(ext, plugin, std::move(promise));
    }

#define DEFINE_WITH_STATE_EXTENSION_LOCAL(TYPE) \
TYPE withStateExtension(TYPE defaultValue, std::function<TYPE(aap_state_extension_t*, AndroidAudioPlugin* plugin)> func) override { return withPluginExtension<TYPE, aap_state_extension_t>(true, defaultValue, AAP_STATE_EXTENSION_URI, func); }

    DEFINE_WITH_STATE_EXTENSION_LOCAL(size_t)
    DEFINE_WITH_STATE_EXTENSION_LOCAL(const aap_state_t&)
    void withStateExtension(std::function<void(aap_state_extension_t*, AndroidAudioPlugin* plugin)> func) override {
        withPluginExtension<int, aap_state_extension_t>(true, 0, AAP_STATE_EXTENSION_URI, [&](aap_state_extension_t* ext, AndroidAudioPlugin* plugin) {
            func(ext, plugin);
            return 0;
        });
    }

#define DEFINE_WITH_PRESETS_EXTENSION_LOCAL_ASYNC(TYPE) \
void withPresetsExtensionAsync(std::promise<TYPE> promise, TYPE defaultValue, std::function<void(aap_presets_extension_t*, AndroidAudioPlugin*, std::promise<TYPE>)> func) override { \
    withPluginExtensionAsync<TYPE, aap_presets_extension_t>(true, std::move(promise), defaultValue, AAP_PRESETS_EXTENSION_URI, func); }

#define DEFINE_WITH_PRESETS_EXTENSION_LOCAL(TYPE) \
TYPE withPresetsExtension(TYPE defaultValue, std::function<TYPE(aap_presets_extension_t*, AndroidAudioPlugin* plugin)> func) override { return withPluginExtension<TYPE, aap_presets_extension_t>(true, defaultValue, AAP_PRESETS_EXTENSION_URI, func); }

    DEFINE_WITH_PRESETS_EXTENSION_LOCAL(int32_t)
    DEFINE_WITH_PRESETS_EXTENSION_LOCAL(std::string)
    DEFINE_WITH_PRESETS_EXTENSION_LOCAL_ASYNC(std::string)
    void withPresetsExtension(std::function<void(aap_presets_extension_t*, AndroidAudioPlugin*)> func) override {
        withPluginExtension<int, aap_presets_extension_t>(true, 0, AAP_STATE_EXTENSION_URI, [&](aap_presets_extension_t* ext, AndroidAudioPlugin* plugin) {
            func(ext, plugin);
            return 0;
        });
    }


#define DEFINE_WITH_PARAMETERS_EXTENSION_LOCAL(TYPE) \
TYPE withParametersExtension(TYPE dummyValue, std::function<TYPE(aap_parameters_extension_t*, AndroidAudioPlugin* plugin)> func) override { return withPluginExtension<TYPE, aap_parameters_extension_t>(false, 0, AAP_PARAMETERS_EXTENSION_URI, func); }

    DEFINE_WITH_PARAMETERS_EXTENSION_LOCAL(int32_t)
    virtual void withParametersExtension(std::function<void(aap_parameters_extension_t*, AndroidAudioPlugin*)> func) override {
        withPluginExtension<int, aap_parameters_extension_t>(false, 0, AAP_PARAMETERS_EXTENSION_URI, [&](aap_parameters_extension_t* ext, AndroidAudioPlugin* plugin) {
            func(ext, plugin);
            return 0;
        });
    }

#define DEFINE_WITH_MIDI_EXTENSION_LOCAL(TYPE) \
TYPE withMidiExtension(TYPE dummyValue, std::function<TYPE(aap_midi_extension_t*, AndroidAudioPlugin* plugin)> func) override { return withPluginExtension<TYPE, aap_midi_extension_t>(false, 0, AAP_MIDI_EXTENSION_URI, func); }

    DEFINE_WITH_MIDI_EXTENSION_LOCAL(int32_t)
    virtual void withMidiExtension(std::function<void(aap_midi_extension_t*, AndroidAudioPlugin*)> func) override {
        withPluginExtension<int, aap_midi_extension_t>(false, 0, AAP_MIDI_EXTENSION_URI, [&](aap_midi_extension_t* ext, AndroidAudioPlugin* plugin) {
            func(ext, plugin);
            return 0;
        });
    }

#define DEFINE_WITH_GUI_EXTENSION_LOCAL(TYPE) \
TYPE withGuiExtension(TYPE dummyValue, std::function<TYPE(aap_gui_extension_t*, AndroidAudioPlugin* plugin)> func) override { return withPluginExtension<TYPE, aap_gui_extension_t>(false, 0, AAP_GUI_EXTENSION_URI, func); }

    DEFINE_WITH_GUI_EXTENSION_LOCAL(int32_t)
    virtual void withGuiExtension(std::function<void(aap_gui_extension_t*, AndroidAudioPlugin*)> func) override {
        withPluginExtension<int, aap_gui_extension_t>(false, 0, AAP_GUI_EXTENSION_URI, [&](aap_gui_extension_t* ext, AndroidAudioPlugin* plugin) {
            func(ext, plugin);
            return 0;
        });
    }

public:
    virtual AndroidAudioPlugin* getPlugin() = 0;
};

class RemotePluginInstanceStandardExtensions : public StandardExtensions {
    bool supportsAsynchronousCalls() override { return true; }

    template<typename T, typename X> T withPluginExtension(bool mandatory, T defaultValue, const char* extensionUri, std::function<T(X*, AndroidAudioPlugin* plugin)> func) {
        auto proxyContext = getAAPXSManager()->getExtensionProxy(extensionUri);
        auto ext = (X*) proxyContext.extension;
        if (ext == nullptr && mandatory)
            return defaultValue;
        return func(ext, getPlugin());
    }
    template<typename T, typename X> void withPluginExtensionAsync(bool mandatory, std::promise<T> promise, T defaultValue, const char* extensionUri, std::function<void(X*, AndroidAudioPlugin*, std::promise<T>)> func) {
        auto plugin = getPlugin();
        auto ext = (X*) plugin->get_extension(plugin, extensionUri);
        if (ext == nullptr && mandatory)
            promise.set_value(defaultValue);
        else
            func(ext, plugin, std::move(promise));
    }

#define AAP_INTERNAL_DEFINE_WITH_STATE_EXTENSION_REMOTE(TYPE) \
TYPE withStateExtension(TYPE defaultValue, std::function<TYPE(aap_state_extension_t*, AndroidAudioPlugin* plugin)> func) override { return withPluginExtension<TYPE, aap_state_extension_t>(true, defaultValue, AAP_STATE_EXTENSION_URI, func); }

    AAP_INTERNAL_DEFINE_WITH_STATE_EXTENSION_REMOTE(size_t)
    AAP_INTERNAL_DEFINE_WITH_STATE_EXTENSION_REMOTE(const aap_state_t&)
    void withStateExtension(std::function<void(aap_state_extension_t*, AndroidAudioPlugin* plugin)> func) override {
        withPluginExtension<int, aap_state_extension_t>(true, 0, AAP_STATE_EXTENSION_URI, [&](aap_state_extension_t* ext, AndroidAudioPlugin* plugin) {
            func(ext, plugin);
            return 0;
        });
    }

#define AAP_INTERNAL_DEFINE_WITH_PRESETS_EXTENSION_REMOTE_ASYNC(TYPE) \
void withPresetsExtensionAsync(std::promise<TYPE> promise, TYPE defaultValue, std::function<void(aap_presets_extension_t*, AndroidAudioPlugin*, std::promise<TYPE>)> func) override { withPluginExtensionAsync<TYPE, aap_presets_extension_t>(true, std::move(promise), defaultValue, AAP_PRESETS_EXTENSION_URI, func); }

#define AAP_INTERNAL_DEFINE_WITH_PRESETS_EXTENSION_REMOTE(TYPE) \
TYPE withPresetsExtension(TYPE defaultValue, std::function<TYPE(aap_presets_extension_t*, AndroidAudioPlugin* plugin)> func) override { return withPluginExtension<TYPE, aap_presets_extension_t>(true, defaultValue, AAP_PRESETS_EXTENSION_URI, func); }

    AAP_INTERNAL_DEFINE_WITH_PRESETS_EXTENSION_REMOTE(int32_t)
    AAP_INTERNAL_DEFINE_WITH_PRESETS_EXTENSION_REMOTE(std::string)
    AAP_INTERNAL_DEFINE_WITH_PRESETS_EXTENSION_REMOTE_ASYNC(std::string)
    virtual void withPresetsExtension(std::function<void(aap_presets_extension_t*, AndroidAudioPlugin*)> func) override {
        withPluginExtension<int, aap_presets_extension_t>(true, 0, AAP_STATE_EXTENSION_URI, [&](aap_presets_extension_t* ext, AndroidAudioPlugin* plugin) {
            func(ext, plugin);
            return 0;
        });
    }

#define AAP_INTERNAL_DEFINE_WITH_PARAMETERS_EXTENSION_REMOTE(TYPE) \
TYPE withParametersExtension(TYPE dummyValue, std::function<TYPE(aap_parameters_extension_t*, AndroidAudioPlugin* plugin)> func) override { return withPluginExtension<TYPE, aap_parameters_extension_t>(false, 0, AAP_PARAMETERS_EXTENSION_URI, func); }

    AAP_INTERNAL_DEFINE_WITH_PARAMETERS_EXTENSION_REMOTE(int32_t)
    virtual void withParametersExtension(std::function<void(aap_parameters_extension_t*, AndroidAudioPlugin*)> func) override {
        withPluginExtension<int, aap_parameters_extension_t>(false, 0, AAP_PARAMETERS_EXTENSION_URI, [&](aap_parameters_extension_t* ext, AndroidAudioPlugin* plugin) {
            func(ext, plugin);
            return 0;
        });
    }

#define AAP_INTERNAL_DEFINE_WITH_MIDI_EXTENSION_REMOTE(TYPE) \
TYPE withMidiExtension(TYPE dummyValue, std::function<TYPE(aap_midi_extension_t*, AndroidAudioPlugin*)> func) override { return withPluginExtension<TYPE, aap_midi_extension_t>(false, 0, AAP_MIDI_EXTENSION_URI, func); }

    AAP_INTERNAL_DEFINE_WITH_MIDI_EXTENSION_REMOTE(int32_t)
    virtual void withMidiExtension(std::function<void(aap_midi_extension_t*, AndroidAudioPlugin*)> func) override {
        withPluginExtension<int, aap_midi_extension_t>(false, 0, AAP_MIDI_EXTENSION_URI, [&](aap_midi_extension_t* ext, AndroidAudioPlugin* plugin) {
            func(ext, plugin);
            return 0;
        });
    }

#define AAP_INTERNAL_DEFINE_WITH_GUI_EXTENSION_REMOTE(TYPE) \
TYPE withGuiExtension(TYPE dummyValue, std::function<TYPE(aap_gui_extension_t*, AndroidAudioPlugin* plugin)> func) override { return withPluginExtension<TYPE, aap_gui_extension_t>(false, 0, AAP_GUI_EXTENSION_URI, func); }

    AAP_INTERNAL_DEFINE_WITH_GUI_EXTENSION_REMOTE(int32_t)
    virtual void withGuiExtension(std::function<void(aap_gui_extension_t*, AndroidAudioPlugin*)> func) override {
        withPluginExtension<int, aap_gui_extension_t>(false, 0, AAP_GUI_EXTENSION_URI, [&](aap_gui_extension_t* ext, AndroidAudioPlugin* plugin) {
            func(ext, plugin);
            return 0;
        });
    }

public:
    virtual AndroidAudioPlugin* getPlugin() = 0;
    virtual AAPXSClientInstanceManager* getAAPXSManager() = 0;
};

}

#endif //AAP_CORE_STANDARD_EXTENSIONS_H
