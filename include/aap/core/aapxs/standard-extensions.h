
#ifndef AAP_CORE_STANDARD_EXTENSIONS_V2_H
#define AAP_CORE_STANDARD_EXTENSIONS_V2_H

#include "aapxs-hosting-runtime.h"
#include "presets-aapxs.h"
#include "parameters-aapxs.h"
#include "state-aapxs.h"
#include "midi-aapxs.h"
#include "gui-aapxs.h"
#include "urid-aapxs.h"

namespace aap::xs {
    class StandardExtensions {
        aap_state_t tmp_state{nullptr, 0};
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

        // Presets
        virtual int32_t getPresetCount() = 0;
        virtual void getPreset(int32_t index, aap_preset_t& preset) = 0;
        virtual std::string getPresetName(int32_t index) = 0;
        virtual int32_t getCurrentPresetIndex() = 0;
        virtual void setCurrentPresetIndex(int32_t index) = 0;

        // State
        virtual int32_t getStateSize() = 0;
        virtual aap_state_t getState() = 0;
        virtual void setState(aap_state_t& stateToLoad) = 0;
        void setState(void* stateToLoad, int32_t dataSize) {
            if (tmp_state.data_size < dataSize) {
                if (tmp_state.data)
                    free(tmp_state.data);
                tmp_state.data = calloc(1, dataSize);
            }
            memcpy(tmp_state.data, stateToLoad, dataSize);
            setState(tmp_state);
        }

        // Gui
        virtual aap_gui_instance_id createGui(std::string pluginId, int32_t instanceId, void* audioPluginView) = 0;
        virtual int32_t showGui(aap_gui_instance_id guiInstanceId) = 0;
        virtual int32_t hideGui(aap_gui_instance_id guiInstanceId) = 0;
        virtual int32_t resizeGui(aap_gui_instance_id guiInstanceId, int32_t width, int32_t height) = 0;
        virtual int32_t destroyGui(aap_gui_instance_id guiInstanceId) = 0;
    };

    class ClientStandardExtensions : public StandardExtensions {
        std::unique_ptr<MidiClientAAPXS> midi{nullptr};
        std::unique_ptr<ParametersClientAAPXS> parameters{nullptr};
        std::unique_ptr<PresetsClientAAPXS> presets{nullptr};
        std::unique_ptr<StateClientAAPXS> state{nullptr};
        std::unique_ptr<GuiClientAAPXS> gui{nullptr};

    public:
        void initialize(AAPXSClientDispatcher* dispatcher) {
            midi = std::make_unique<MidiClientAAPXS>(dispatcher->getPluginAAPXSByUri(AAP_MIDI_EXTENSION_URI), dispatcher->getSerialization(AAP_MIDI_EXTENSION_URI));
            parameters = std::make_unique<ParametersClientAAPXS>(dispatcher->getPluginAAPXSByUri(AAP_PARAMETERS_EXTENSION_URI), dispatcher->getSerialization(AAP_PARAMETERS_EXTENSION_URI));
            presets = std::make_unique<PresetsClientAAPXS>(dispatcher->getPluginAAPXSByUri(AAP_PRESETS_EXTENSION_URI), dispatcher->getSerialization(AAP_PRESETS_EXTENSION_URI));
            state = std::make_unique<StateClientAAPXS>(dispatcher->getPluginAAPXSByUri(AAP_STATE_EXTENSION_URI), dispatcher->getSerialization(AAP_STATE_EXTENSION_URI));
            gui = std::make_unique<GuiClientAAPXS>(dispatcher->getPluginAAPXSByUri(AAP_GUI_EXTENSION_URI), dispatcher->getSerialization(AAP_GUI_EXTENSION_URI));
        }

        // MIDI
        int32_t getMidiMappingPolicy() override { return midi->getMidiMappingPolicy(); }

        // Parameters
        int32_t getParameterCount() override { return parameters->getParameterCount(); }
        aap_parameter_info_t getParameter(int32_t index) override { return parameters->getParameter(index); }
        double getParameterProperty(int32_t index, int32_t propertyId) override { return parameters->getProperty(index, propertyId); }
        int32_t getEnumerationCount(int32_t index) override { return parameters->getEnumerationCount(index); }
        aap_parameter_enum_t getEnumeration(int32_t index, int32_t enumIndex) override { return parameters->getEnumeration(index, enumIndex); }

        // Presets
        int32_t getPresetCount() override { return presets->getPresetCount(); }
        void getPreset(int32_t index, aap_preset_t& preset) override { presets->getPreset(index, preset); }
        std::string getPresetName(int32_t index) override {
            aap_preset_t preset;
            presets->getPreset(index, preset);
            return preset.name;
        }
        int32_t getCurrentPresetIndex() override { return presets->getPresetIndex(); }
        void setCurrentPresetIndex(int32_t index) override { presets->setPresetIndex(index); }

        // State
        int32_t getStateSize() override { return state->getStateSize(); }
        aap_state_t getState() override {
            aap_state_t stateToSave;
            state->getState(stateToSave);
            return stateToSave;
        }
        void setState(aap_state_t& stateToLoad) override { return state->setState(stateToLoad); }

        // Gui
        aap_gui_instance_id createGui(std::string pluginId, int32_t instanceId, void* audioPluginView) override { return gui->createGui(pluginId, instanceId, audioPluginView); }
        int32_t showGui(aap_gui_instance_id guiInstanceId) override { return gui->showGui(guiInstanceId); }
        int32_t hideGui(aap_gui_instance_id guiInstanceId) override { return gui->hideGui(guiInstanceId); }
        int32_t resizeGui(aap_gui_instance_id guiInstanceId, int32_t width, int32_t height) override { return gui->resizeGui(guiInstanceId, width, height); }
        int32_t destroyGui(aap_gui_instance_id guiInstanceId) override { return gui->destroyGui(guiInstanceId); }
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
        int32_t getParameterCount() override { return parameters ? parameters->get_parameter_count(parameters, plugin) : 0; }
        aap_parameter_info_t getParameter(int32_t index) override { return parameters ? parameters->get_parameter(parameters, plugin, index) : aap_parameter_info_t{}; }
        double getParameterProperty(int32_t index, int32_t propertyId) override { return parameters ? parameters->get_parameter_property(parameters, plugin, index, propertyId) : 0.0; }
        int32_t getEnumerationCount(int32_t index) override { return parameters ? parameters->get_enumeration_count(parameters, plugin, index) : 0; }
        aap_parameter_enum_t getEnumeration(int32_t index, int32_t enumIndex) override { return parameters ? parameters->get_enumeration(parameters, plugin, index, enumIndex) : aap_parameter_enum_t{}; }

        // Presets
        int32_t getPresetCount() override { return presets ? presets->get_preset_count(presets, plugin) : 0; }
        void getPreset(int32_t index, aap_preset_t& preset) override { if (presets) presets->get_preset(presets, plugin, index, &preset, nullptr, nullptr); }
        std::string getPresetName(int32_t index) override {
            if (!presets)
                return "";
            aap_preset_t preset;
            getPreset(index, preset);
            return preset.name;
        }
        int32_t getCurrentPresetIndex() override { return presets ? presets->get_preset_index(presets, plugin) : 0; }
        void setCurrentPresetIndex(int32_t index) override { if (presets) presets->set_preset_index(presets, plugin, index); }

        // State
        int32_t getStateSize() override { return state ? state->get_state_size(state, plugin) : 0; }
        aap_state_t getState() override {
            aap_state_t stateToSave{};
            if (state)
                state->get_state(state, plugin, &stateToSave);
            return stateToSave;
        }
        void setState(aap_state_t& stateToLoad) override { if (state) state->set_state(state, plugin, &stateToLoad); }

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
