#ifndef AAP_CORE_STANDARD_EXTENSIONS_H
#define AAP_CORE_STANDARD_EXTENSIONS_H

#include "extension-service.h"
#include "aap/ext/midi.h"
#include "aap/ext/state.h"
#include "aap/ext/presets.h"

// FIXME: should this be moved to somewhere?
extern "C" int32_t getMidiSettingsFromSharedPreference(std::string pluginId);

namespace aap {

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

#define DEFINE_WITH_PARAMETERS_EXTENSION(TYPE) \
virtual TYPE withParametersExtension(TYPE dummyValue, std::function<TYPE(aap_parameters_extension_t*, AndroidAudioPluginExtensionTarget target)> func) = 0;

    DEFINE_WITH_PARAMETERS_EXTENSION(int32_t)
    virtual void withParametersExtension(std::function<void(aap_parameters_extension_t*, AndroidAudioPluginExtensionTarget)> func) = 0;

#define DEFINE_WITH_MIDI_EXTENSION(TYPE) \
virtual TYPE withMidiExtension(TYPE dummyValue, std::function<TYPE(aap_midi_extension_t*, AndroidAudioPluginExtensionTarget target)> func) = 0;

    DEFINE_WITH_MIDI_EXTENSION(int32_t)
    virtual void withMidiExtension(std::function<void(aap_midi_extension_t*, AndroidAudioPluginExtensionTarget)> func) = 0;

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

    std::string getPresetName(int index)
    {
        return withPresetsExtension/*<std::string>*/("", [&](aap_presets_extension_t* ext, AndroidAudioPluginExtensionTarget target) {
            aap_preset_t result;
            ext->get_preset(target, index, true, &result);
            return std::string{result.name};
        });
    }

    int32_t getMidiMappingPolicy(std::string pluginId) {
        return withMidiExtension(0, [&](aap_midi_extension_t* ext, AndroidAudioPluginExtensionTarget target) {
            if (ext && ext->get_mapping_policy)
                return (int32_t) ext->get_mapping_policy(target, pluginId.c_str());
            else
                return (int32_t) AAP_PARAMETERS_MAPPING_POLICY_SYSEX8;
        });
    }
};

class LocalPluginInstanceStandardExtensions : public StandardExtensions {
    template<typename T, typename X> T withExtension(bool mandatory, T defaultValue, const char* extensionUri, std::function<T(X*, AndroidAudioPluginExtensionTarget target)> func) {
        auto plugin = getPlugin();
        auto ext = (X*) plugin->get_extension(plugin, extensionUri);
        if (ext == nullptr && mandatory)
            return defaultValue;
        return func(ext, AndroidAudioPluginExtensionTarget{plugin, nullptr});
    }

#define DEFINE_WITH_STATE_EXTENSION_LOCAL(TYPE) \
TYPE withStateExtension(TYPE defaultValue, std::function<TYPE(aap_state_extension_t*, AndroidAudioPluginExtensionTarget target)> func) override { return withExtension<TYPE, aap_state_extension_t>(true, defaultValue, AAP_STATE_EXTENSION_URI, func); }

    DEFINE_WITH_STATE_EXTENSION_LOCAL(size_t)
    DEFINE_WITH_STATE_EXTENSION_LOCAL(const aap_state_t&)
    void withStateExtension(std::function<void(aap_state_extension_t*, AndroidAudioPluginExtensionTarget target)> func) override {
        withExtension<int, aap_state_extension_t>(true, 0, AAP_STATE_EXTENSION_URI, [&](aap_state_extension_t* ext, AndroidAudioPluginExtensionTarget target) {
            func(ext, target);
            return 0;
        });
    }

#define DEFINE_WITH_PRESETS_EXTENSION_LOCAL(TYPE) \
TYPE withPresetsExtension(TYPE defaultValue, std::function<TYPE(aap_presets_extension_t*, AndroidAudioPluginExtensionTarget target)> func) override { return withExtension<TYPE, aap_presets_extension_t>(true, defaultValue, AAP_PRESETS_EXTENSION_URI, func); }

    DEFINE_WITH_PRESETS_EXTENSION_LOCAL(int32_t)
    DEFINE_WITH_PRESETS_EXTENSION_LOCAL(std::string)
    virtual void withPresetsExtension(std::function<void(aap_presets_extension_t*, AndroidAudioPluginExtensionTarget)> func) override {
        withExtension<int, aap_presets_extension_t>(true, 0, AAP_STATE_EXTENSION_URI, [&](aap_presets_extension_t* ext, AndroidAudioPluginExtensionTarget target) {
            func(ext, target);
            return 0;
        });
    }


#define DEFINE_WITH_PARAMETERS_EXTENSION_LOCAL(TYPE) \
TYPE withParametersExtension(TYPE dummyValue, std::function<TYPE(aap_parameters_extension_t*, AndroidAudioPluginExtensionTarget target)> func) override { return withExtension<TYPE, aap_parameters_extension_t>(false, 0, AAP_PARAMETERS_EXTENSION_URI, func); }

    DEFINE_WITH_PARAMETERS_EXTENSION_LOCAL(int32_t)
    virtual void withParametersExtension(std::function<void(aap_parameters_extension_t*, AndroidAudioPluginExtensionTarget)> func) override {
        withExtension<int, aap_parameters_extension_t>(false, 0, AAP_PARAMETERS_EXTENSION_URI, [&](aap_parameters_extension_t* ext, AndroidAudioPluginExtensionTarget target) {
            func(ext, target);
            return 0;
        });
    }

#define DEFINE_WITH_MIDI_EXTENSION_LOCAL(TYPE) \
TYPE withMidiExtension(TYPE dummyValue, std::function<TYPE(aap_midi_extension_t*, AndroidAudioPluginExtensionTarget target)> func) override { return withExtension<TYPE, aap_midi_extension_t>(false, 0, AAP_PARAMETERS_EXTENSION_URI, func); }

    DEFINE_WITH_MIDI_EXTENSION_LOCAL(int32_t)
    virtual void withMidiExtension(std::function<void(aap_midi_extension_t*, AndroidAudioPluginExtensionTarget)> func) override {
        withExtension<int, aap_midi_extension_t>(false, 0, AAP_MIDI_EXTENSION_URI, [&](aap_midi_extension_t* ext, AndroidAudioPluginExtensionTarget target) {
            func(ext, target);
            return 0;
        });
    }

public:
    virtual AndroidAudioPlugin* getPlugin() = 0;
};

class RemotePluginInstanceStandardExtensions : public StandardExtensions {
    template<typename T, typename X> T withExtension(bool mandatory, T defaultValue, const char* extensionUri, std::function<T(X*, AndroidAudioPluginExtensionTarget target)> func) {
        auto proxyContext = getAAPXSManager()->getExtensionProxy(extensionUri);
        auto ext = (X*) proxyContext.extension;
        if (ext == nullptr && mandatory)
            return defaultValue;
        AndroidAudioPluginExtensionTarget target;
        target.aapxs_context = proxyContext.aapxs_context; // it should be PluginClientExtension::Instance
        target.plugin = getPlugin();
        return func(ext, target);
    }

#define DEFINE_WITH_STATE_EXTENSION_REMOTE(TYPE) \
TYPE withStateExtension(TYPE defaultValue, std::function<TYPE(aap_state_extension_t*, AndroidAudioPluginExtensionTarget target)> func) override { return withExtension<TYPE, aap_state_extension_t>(true, defaultValue, AAP_STATE_EXTENSION_URI, func); }

    DEFINE_WITH_STATE_EXTENSION_REMOTE(size_t)
    DEFINE_WITH_STATE_EXTENSION_REMOTE(const aap_state_t&)
    void withStateExtension(std::function<void(aap_state_extension_t*, AndroidAudioPluginExtensionTarget target)> func) override {
        withExtension<int, aap_state_extension_t>(true, 0, AAP_STATE_EXTENSION_URI, [&](aap_state_extension_t* ext, AndroidAudioPluginExtensionTarget target) {
            func(ext, target);
            return 0;
        });
    }

#define DEFINE_WITH_PRESETS_EXTENSION_REMOTE(TYPE) \
TYPE withPresetsExtension(TYPE defaultValue, std::function<TYPE(aap_presets_extension_t*, AndroidAudioPluginExtensionTarget target)> func) override { return withExtension<TYPE, aap_presets_extension_t>(true, defaultValue, AAP_PRESETS_EXTENSION_URI, func); }

    DEFINE_WITH_PRESETS_EXTENSION_REMOTE(int32_t)
    DEFINE_WITH_PRESETS_EXTENSION_REMOTE(std::string)
    virtual void withPresetsExtension(std::function<void(aap_presets_extension_t*, AndroidAudioPluginExtensionTarget)> func) override {
        withExtension<int, aap_presets_extension_t>(true, 0, AAP_STATE_EXTENSION_URI, [&](aap_presets_extension_t* ext, AndroidAudioPluginExtensionTarget target) {
            func(ext, target);
            return 0;
        });
    }

#define DEFINE_WITH_PARAMETERS_EXTENSION_REMOTE(TYPE) \
TYPE withParametersExtension(TYPE dummyValue, std::function<TYPE(aap_parameters_extension_t*, AndroidAudioPluginExtensionTarget target)> func) override { return withExtension<TYPE, aap_parameters_extension_t>(false, 0, AAP_PARAMETERS_EXTENSION_URI, func); }

    DEFINE_WITH_PARAMETERS_EXTENSION_REMOTE(int32_t)
    virtual void withParametersExtension(std::function<void(aap_parameters_extension_t*, AndroidAudioPluginExtensionTarget)> func) override {
        withExtension<int, aap_parameters_extension_t>(false, 0, AAP_PARAMETERS_EXTENSION_URI, [&](aap_parameters_extension_t* ext, AndroidAudioPluginExtensionTarget target) {
            func(ext, target);
            return 0;
        });
    }

#define DEFINE_WITH_MIDI_EXTENSION_REMOTE(TYPE) \
TYPE withMidiExtension(TYPE dummyValue, std::function<TYPE(aap_midi_extension_t*, AndroidAudioPluginExtensionTarget target)> func) override { return withExtension<TYPE, aap_midi_extension_t>(false, 0, AAP_MIDI_EXTENSION_URI, func); }

    DEFINE_WITH_MIDI_EXTENSION_REMOTE(int32_t)
    virtual void withMidiExtension(std::function<void(aap_midi_extension_t*, AndroidAudioPluginExtensionTarget)> func) override {
        withExtension<int, aap_midi_extension_t>(false, 0, AAP_MIDI_EXTENSION_URI, [&](aap_midi_extension_t* ext, AndroidAudioPluginExtensionTarget target) {
            func(ext, target);
            return 0;
        });
    }

public:
    virtual AndroidAudioPlugin* getPlugin() = 0;
    virtual AAPXSClientInstanceManager* getAAPXSManager() = 0;
};

}

#endif //AAP_CORE_STANDARD_EXTENSIONS_H
