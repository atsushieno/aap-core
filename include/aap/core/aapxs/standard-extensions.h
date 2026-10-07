
#ifndef AAP_CORE_STANDARD_EXTENSIONS_V2_H
#define AAP_CORE_STANDARD_EXTENSIONS_V2_H

#include "aapxs-hosting-runtime.h"
#include "presets-aapxs.h"
#include "parameters-aapxs.h"
#include "state-aapxs.h"
#include "midi-aapxs.h"
#include "gui-aapxs.h"
#include "urid-aapxs.h"
#include <functional>

namespace aap::xs {
    class StandardExtensions {
    protected:
        aap_state_t tmp_state{nullptr, 0};
        size_t tmp_state_capacity{0};

    public:
        virtual ~StandardExtensions() {
            if (tmp_state.data)
                free(tmp_state.data);
        }

        virtual int32_t getMidiMappingPolicy() = 0;

        // Parameters
        virtual int32_t getParameterCount() = 0;
        virtual aap_parameter_info_t getParameter(int32_t index) = 0;
        virtual double getParameterProperty(int32_t index, int32_t propertyId) = 0;
        virtual int32_t getEnumerationCount(int32_t index) = 0;
        virtual aap_parameter_enum_t getEnumeration(int32_t index, int32_t enumIndex) = 0;
        virtual aap_parameters_extension_t* asParametersExtension() { return nullptr; }

        // Presets
        virtual int32_t getPresetCount() = 0;
        virtual Result<bool> getPreset(int32_t index, aap_preset_t& preset) = 0;
        virtual std::string getPresetName(int32_t index) = 0;
        virtual Result<bool> setCurrentPresetIndex(int32_t index) = 0;
        virtual int32_t getPresetAsync(int32_t index, std::function<void(Result<aap_preset_t>)> callback) = 0;
        virtual int32_t setPresetIndexAsync(int32_t index, std::function<void(Result<bool>)> callback) = 0;
        virtual aap_presets_extension_t* asPresetsExtension() { return nullptr; }

        // State
        virtual Result<size_t> getStateSize() = 0;
        virtual Result<aap_state_t> getState() = 0;
        virtual Result<bool> setState(aap_state_t& stateToLoad) = 0;
        virtual int32_t requestStateAsync(std::function<void(Result<aap_state_t>)> callback) = 0;
        virtual int32_t setStateAsync(aap_state_t& stateToLoad, std::function<void(Result<bool>)> callback) = 0;
        Result<bool> setState(void* stateToLoad, int32_t dataSize) {
            if (aap::RealtimeScope::isActive()) return {false, "RT caller"};
            if (tmp_state_capacity < static_cast<size_t>(dataSize)) {
                if (tmp_state.data)
                    free(tmp_state.data);
                tmp_state.data = calloc(1, dataSize);
                tmp_state_capacity = dataSize;
            }
            tmp_state.data_size = dataSize;
            memcpy(tmp_state.data, stateToLoad, dataSize);
            return setState(tmp_state);
        }

        // Gui
        virtual aap_gui_instance_id createGui(std::string pluginId, int32_t instanceId, void* audioPluginView) = 0;
        virtual int32_t showGui(aap_gui_instance_id guiInstanceId) = 0;
        virtual int32_t hideGui(aap_gui_instance_id guiInstanceId) = 0;
        virtual int32_t resizeGui(aap_gui_instance_id guiInstanceId, int32_t width, int32_t height) = 0;
        virtual int32_t destroyGui(aap_gui_instance_id guiInstanceId) = 0;
    };

    class ClientStandardExtensions : public StandardExtensions {
        bool initialized{false};
        MidiClientAAPXS* midi{nullptr};
        ParametersClientAAPXS* parameters{nullptr};
        PresetsClientAAPXS* presets{nullptr};
        StateClientAAPXS* state{nullptr};
        GuiClientAAPXS* gui{nullptr};
        UridClientAAPXS* urid{nullptr};

    public:
        // These clients are owned by their AAPXS instance contexts. Hosting only borrows them.
        void* asNativePluginExtension(const char* uri) {
            if (!uri) return nullptr;
            if (!strcmp(uri, AAP_MIDI_EXTENSION_URI)) return midi ? midi->asPluginExtension() : nullptr;
            if (!strcmp(uri, AAP_PARAMETERS_EXTENSION_URI)) return parameters ? parameters->asPluginExtension() : nullptr;
            if (!strcmp(uri, AAP_PRESETS_EXTENSION_URI)) return presets ? presets->asPluginExtension() : nullptr;
            if (!strcmp(uri, AAP_STATE_EXTENSION_URI)) return state ? state->asPluginExtension() : nullptr;
            if (!strcmp(uri, AAP_URID_EXTENSION_URI)) return urid ? urid->asPluginExtension() : nullptr;
            return nullptr;
        }
        void initialize(AAPXSClientDispatcher* dispatcher) {
            if (initialized)
                return;
            auto borrow = [dispatcher](const char* uri) -> TypedAAPXS* {
                auto* instance = dispatcher->getPluginAAPXSByUri(uri);
                return instance ? static_cast<TypedAAPXS*>(instance->typed_client) : nullptr;
            };
            midi = dynamic_cast<MidiClientAAPXS*>(borrow(AAP_MIDI_EXTENSION_URI));
            parameters = dynamic_cast<ParametersClientAAPXS*>(borrow(AAP_PARAMETERS_EXTENSION_URI));
            presets = dynamic_cast<PresetsClientAAPXS*>(borrow(AAP_PRESETS_EXTENSION_URI));
            state = dynamic_cast<StateClientAAPXS*>(borrow(AAP_STATE_EXTENSION_URI));
            gui = dynamic_cast<GuiClientAAPXS*>(borrow(AAP_GUI_EXTENSION_URI));
            urid = dynamic_cast<UridClientAAPXS*>(borrow(AAP_URID_EXTENSION_URI));
            initialized = true;
        }

        // URID
        void map(uint8_t uridValue, const char* uri) { if (urid) urid->map(uridValue, uri); }

        // MIDI
        int32_t getMidiMappingPolicy() override { return midi ? midi->getMidiMappingPolicy() : AAP_PARAMETERS_MAPPING_POLICY_NONE; }

        // Parameters
        int32_t getParameterCount() override { return parameters ? parameters->getParameterCount() : -1; }
        aap_parameter_info_t getParameter(int32_t index) override { return parameters ? parameters->getParameter(index) : aap_parameter_info_t{}; }
        double getParameterProperty(int32_t index, int32_t propertyId) override { return parameters ? parameters->getProperty(index, propertyId) : 0.0; }
        int32_t getEnumerationCount(int32_t index) override { return parameters ? parameters->getEnumerationCount(index) : 0; }
        aap_parameter_enum_t getEnumeration(int32_t index, int32_t enumIndex) override { return parameters ? parameters->getEnumeration(index, enumIndex) : aap_parameter_enum_t{}; }
        aap_parameters_extension_t* asParametersExtension() override { return parameters ? parameters->asPluginExtension() : nullptr; }

        // Presets
        int32_t getPresetCount() override { return presets ? presets->getPresetCount() : 0; }
        // OBSOLETE: use getPresetAsync() instead.
        Result<bool> getPreset(int32_t index, aap_preset_t& preset) override {
            if (aap::RealtimeScope::isActive()) return {false, "RT caller"};
            if (!presets) return {false, "presets extension unavailable"};
            auto error = presets->getPreset(index, preset);
            return Result<bool>{error.empty(), error};
        }
        std::string getPresetName(int32_t index) override {
            if (!presets) return "";
            aap_preset_t preset{};
            presets->getPreset(index, preset);
            return preset.name;
        }
        // OBSOLETE: use setPresetIndexAsync() instead.
        Result<bool> setCurrentPresetIndex(int32_t index) override {
            if (aap::RealtimeScope::isActive()) return {false, "RT caller"};
            if (!presets) return {false, "presets extension unavailable"};
            auto error = presets->setPresetIndex(index);
            return Result<bool>{error.empty(), error};
        }
        int32_t getPresetAsync(int32_t index, std::function<void(Result<aap_preset_t>)> callback) override {
            if (aap::RealtimeScope::isActive()) return -1;
            if (!presets) { if (callback) callback({{}, "presets extension unavailable"}); return -1; }
            return presets->getPresetAsync(index, std::move(callback));
        }
        int32_t setPresetIndexAsync(int32_t index, std::function<void(Result<bool>)> callback) override {
            if (aap::RealtimeScope::isActive()) return -1;
            if (!presets) { if (callback) callback({{}, "presets extension unavailable"}); return -1; }
            return presets->setPresetIndexAsync(index, std::move(callback));
        }
        aap_presets_extension_t* asPresetsExtension() override { return presets ? presets->asPluginExtension() : nullptr; }

        // State
        Result<size_t> getStateSize() override { if (aap::RealtimeScope::isActive()) return {0, "RT caller"}; return state ? state->getStateSize() : Result<size_t>{0, "state extension unavailable"}; }
        // OBSOLETE: use requestStateAsync() instead.
        Result<aap_state_t> getState() override {
            if (aap::RealtimeScope::isActive()) return {{nullptr, 0}, "RT caller"};
            if (!state) return {{nullptr, 0}, "state extension unavailable"};
            if (tmp_state_capacity < static_cast<size_t>(STATE_SHARED_MEMORY_SIZE)) {
                if (tmp_state.data)
                    free(tmp_state.data);
                tmp_state.data = calloc(1, STATE_SHARED_MEMORY_SIZE);
                tmp_state_capacity = STATE_SHARED_MEMORY_SIZE;
            }
            tmp_state.data_size = tmp_state_capacity;
            auto error = state->getState(tmp_state);
            return Result<aap_state_t>{tmp_state, error};
        }
        // OBSOLETE: use setStateAsync() instead.
        Result<bool> setState(aap_state_t& stateToLoad) override {
            if (aap::RealtimeScope::isActive()) return {false, "RT caller"};
            if (!state) return {false, "state extension unavailable"};
            auto error = state->setState(stateToLoad);
            return Result<bool>{error.empty(), error};
        }
        int32_t requestStateAsync(std::function<void(Result<aap_state_t>)> callback) override {
            if (aap::RealtimeScope::isActive()) return -1;
            if (!state) { if (callback) callback({{}, "state extension unavailable"}); return -1; }
            return state->requestStateAsync(std::move(callback));
        }
        int32_t setStateAsync(aap_state_t& stateToLoad, std::function<void(Result<bool>)> callback) override {
            if (aap::RealtimeScope::isActive()) return -1;
            if (!state) { if (callback) callback({{}, "state extension unavailable"}); return -1; }
            return state->setStateAsync(stateToLoad, std::move(callback));
        }

        // Gui
        aap_gui_instance_id createGui(std::string pluginId, int32_t instanceId, void* audioPluginView) override { return gui ? gui->createGui(pluginId, instanceId, audioPluginView) : -1; }
        int32_t showGui(aap_gui_instance_id guiInstanceId) override { return gui ? gui->showGui(guiInstanceId) : -1; }
        int32_t hideGui(aap_gui_instance_id guiInstanceId) override { return gui ? gui->hideGui(guiInstanceId) : -1; }
        int32_t resizeGui(aap_gui_instance_id guiInstanceId, int32_t width, int32_t height) override { return gui ? gui->resizeGui(guiInstanceId, width, height) : -1; }
        int32_t destroyGui(aap_gui_instance_id guiInstanceId) override { return gui ? gui->destroyGui(guiInstanceId) : -1; }
    };

    class ServiceStandardExtensions : public StandardExtensions {
        AndroidAudioPlugin* plugin;
        aap_midi_extension_t* midi;
        aap_parameters_extension_t* parameters;
        aap_presets_extension_t* presets;
        aap_state_extension_t* state;
        aap_gui_extension_t* gui;

    public:
        ServiceStandardExtensions(AndroidAudioPlugin* plugin) : plugin(plugin) {
            midi = (aap_midi_extension_t*) plugin->get_extension(plugin, AAP_MIDI_EXTENSION_URI);
            parameters = (aap_parameters_extension_t*) plugin->get_extension(plugin, AAP_PARAMETERS_EXTENSION_URI);
            presets = (aap_presets_extension_t*) plugin->get_extension(plugin, AAP_PRESETS_EXTENSION_URI);
            state = (aap_state_extension_t*) plugin->get_extension(plugin, AAP_STATE_EXTENSION_URI);
        }

        // MIDI
        int32_t getMidiMappingPolicy() override { return midi ? midi->get_mapping_policy(midi, plugin) : 0; }

        // Parameters
        int32_t getParameterCount() override { return parameters ? parameters->get_parameter_count(parameters, plugin) : -1; }
        aap_parameter_info_t getParameter(int32_t index) override { return parameters ? parameters->get_parameter(parameters, plugin, index) : aap_parameter_info_t{}; }
        double getParameterProperty(int32_t index, int32_t propertyId) override { return parameters ? parameters->get_parameter_property(parameters, plugin, index, propertyId) : 0.0; }
        int32_t getEnumerationCount(int32_t index) override { return parameters ? parameters->get_enumeration_count(parameters, plugin, index) : 0; }
        aap_parameter_enum_t getEnumeration(int32_t index, int32_t enumIndex) override { return parameters ? parameters->get_enumeration(parameters, plugin, index, enumIndex) : aap_parameter_enum_t{}; }

        // Presets
        int32_t getPresetCount() override { return presets ? presets->get_preset_count(presets, plugin) : 0; }
        Result<bool> getPreset(int32_t index, aap_preset_t& preset) override {
            if (!presets)
                return Result<bool>{false, "presets extension not implemented"};
            presets->get_preset(presets, plugin, index, &preset, nullptr, nullptr);
            return Result<bool>{true, ""};
        }
        std::string getPresetName(int32_t index) override {
            if (!presets)
                return "";
            aap_preset_t preset{};
            getPreset(index, preset);
            return preset.name;
        }
        Result<bool> setCurrentPresetIndex(int32_t index) override {
            if (!presets)
                return Result<bool>{false, "presets extension not implemented"};
            presets->set_preset_index(presets, plugin, index);
            return Result<bool>{true, ""};
        }
        // Service side is in-process: complete synchronously and invoke the callback inline.
        int32_t getPresetAsync(int32_t index, std::function<void(Result<aap_preset_t>)> callback) override {
            aap_preset_t preset{};
            auto ret = getPreset(index, preset);
            if (callback)
                callback(Result<aap_preset_t>{preset, ret.error});
            return 0;
        }
        int32_t setPresetIndexAsync(int32_t index, std::function<void(Result<bool>)> callback) override {
            auto ret = setCurrentPresetIndex(index);
            if (callback)
                callback(ret);
            return 0;
        }

        // State
        Result<size_t> getStateSize() override {
            if (!state || !state->get_state_size)
                return {0, "state extension not implemented"};
            return {state->get_state_size(state, plugin), ""};
        }
        Result<aap_state_t> getState() override {
            auto sizeResult = getStateSize();
            if (!sizeResult.isOk())
                return {aap_state_t{nullptr, 0}, sizeResult.error};
            auto stateSize = sizeResult.value;
            if (tmp_state.data_size < static_cast<size_t>(stateSize)) {
                if (tmp_state.data)
                    free(tmp_state.data);
                tmp_state.data = calloc(1, stateSize);
            }
            tmp_state.data_size = stateSize;
            if (state)
                state->get_state(state, plugin, &tmp_state);
            return Result<aap_state_t>{tmp_state, ""};
        }
        Result<bool> setState(aap_state_t& stateToLoad) override {
            if (state)
                state->set_state(state, plugin, &stateToLoad);
            return Result<bool>{true, ""};
        }
        // Service side is in-process: complete synchronously and invoke the callback inline.
        int32_t requestStateAsync(std::function<void(Result<aap_state_t>)> callback) override {
            auto ret = getState();
            if (callback)
                callback(ret);
            return 0;
        }
        int32_t setStateAsync(aap_state_t& stateToLoad, std::function<void(Result<bool>)> callback) override {
            auto ret = setState(stateToLoad);
            if (callback)
                callback(ret);
            return 0;
        }

        // Gui
        aap_gui_instance_id createGui(std::string pluginId, int32_t instanceId, void* audioPluginView) override { return gui ? gui->create(gui, plugin, pluginId.c_str(), instanceId, audioPluginView) : -1; }
        int32_t showGui(aap_gui_instance_id guiInstanceId) override { return gui ? gui->show ? gui->show(gui, plugin, guiInstanceId) : AAP_GUI_ERROR_NO_SHOW_DEFINED : AAP_GUI_ERROR_NO_GUI_DEFINED; }
        int32_t hideGui(aap_gui_instance_id guiInstanceId) override {
            if (gui)
                if (gui->hide)
                    gui->hide(gui, plugin, guiInstanceId);
                else
                    return AAP_GUI_ERROR_NO_HIDE_DEFINED;
            else
                return AAP_GUI_ERROR_NO_GUI_DEFINED;
            return 0;
        }
        int32_t resizeGui(aap_gui_instance_id guiInstanceId, int32_t width, int32_t height) override { return gui ? gui->resize ? gui->resize(gui, plugin, guiInstanceId, width, height) : AAP_GUI_ERROR_NO_RESIZE_DEFINED : AAP_GUI_ERROR_NO_GUI_DEFINED; }
        int32_t destroyGui(aap_gui_instance_id guiInstanceId) override {
            if (gui)
                if (gui->destroy)
                    gui->destroy(gui, plugin, guiInstanceId);
                else
                    return AAP_GUI_ERROR_NO_DESTROY_DEFINED;
            else
                return AAP_GUI_ERROR_NO_GUI_DEFINED;
            return 0;
        }
    };

    class StandardHostExtensions {

        // MIDI

        // Parameters
        virtual void notifyParametersChanged() = 0;

        // Presets
        virtual void notifyPresetLoaded() = 0;
        virtual void notifyPresetsUpdated() = 0;

        // State

        // GUI

    };

    class ClientStandardHostExtensions : public StandardHostExtensions {
        AndroidAudioPluginHost* host;
        aap_parameters_host_extension_t* parameters;
        aap_presets_host_extension_t* presets;

    public:
        ClientStandardHostExtensions(AndroidAudioPluginHost* host) : host(host) {
            parameters = (aap_parameters_host_extension_t*) host->get_extension(host, AAP_PARAMETERS_EXTENSION_URI);
            presets = (aap_presets_host_extension_t*) host->get_extension(host, AAP_PRESETS_EXTENSION_URI);
        }

        // MIDI

        // Parameters
        void notifyParametersChanged() override { parameters->notify_parameters_changed(parameters, host); }

        // Presets
        void notifyPresetLoaded() override { presets->notify_preset_loaded(presets, host); }
        void notifyPresetsUpdated() override { presets->notify_presets_updated(presets, host); }

        // State

        // GUI
    };

    class ServiceStandardHostExtensions : public StandardHostExtensions {
        MidiServiceAAPXS midi;
        ParametersServiceAAPXS parameters;
        PresetsServiceAAPXS presets;
        StateServiceAAPXS state;
        GuiServiceAAPXS gui;

    public:
        ServiceStandardHostExtensions(AAPXSServiceDispatcher* dispatcher, AAPXSSerializationContext* serialization) :
                midi(dispatcher->getHostAAPXSByUri(AAP_MIDI_EXTENSION_URI), serialization),
                parameters(dispatcher->getHostAAPXSByUri(AAP_PARAMETERS_EXTENSION_URI), serialization),
                presets(dispatcher->getHostAAPXSByUri(AAP_PRESETS_EXTENSION_URI), serialization),
                state(dispatcher->getHostAAPXSByUri(AAP_STATE_EXTENSION_URI), serialization),
                gui(dispatcher->getHostAAPXSByUri(AAP_GUI_EXTENSION_URI), serialization) {
        }

        // MIDI

        // Parameters
        void notifyParametersChanged() override { parameters.notifyParametersChanged(); }

        // Presets
        void notifyPresetLoaded() override { presets.notifyPresetLoaded(); }
        void notifyPresetsUpdated() override { presets.notifyPresetsUpdated(); }

        // State

        // GUI
    };
}

#endif //AAP_CORE_STANDARD_EXTENSIONS_V2_H
