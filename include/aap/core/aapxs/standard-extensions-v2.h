
#ifndef AAP_CORE_STANDARD_EXTENSIONS_V2_H
#define AAP_CORE_STANDARD_EXTENSIONS_V2_H

#include "presets-aapxs.h"
#include "parameters-aapxs.h"
#include "state-aapxs.h"
#include "midi-aapxs.h"
#include "gui-aapxs.h"

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
        MidiClientAAPXS midi;
        ParametersClientAAPXS parameters;
        PresetsClientAAPXS presets;
        StateClientAAPXS state;
        GuiClientAAPXS gui;

    public:
        ClientStandardExtensions(AAPXSClientDispatcher* dispatcher) :
                midi(dispatcher->getPluginAAPXSByUri(AAP_MIDI_EXTENSION_URI), dispatcher->getSerialization(AAP_MIDI_EXTENSION_URI)),
                parameters(dispatcher->getPluginAAPXSByUri(AAP_PARAMETERS_EXTENSION_URI), dispatcher->getSerialization(AAP_PARAMETERS_EXTENSION_URI)),
                presets(dispatcher->getPluginAAPXSByUri(AAP_PRESETS_EXTENSION_URI), dispatcher->getSerialization(AAP_PRESETS_EXTENSION_URI)),
                state(dispatcher->getPluginAAPXSByUri(AAP_STATE_EXTENSION_URI), dispatcher->getSerialization(AAP_STATE_EXTENSION_URI)),
                gui(dispatcher->getPluginAAPXSByUri(AAP_GUI_EXTENSION_URI), dispatcher->getSerialization(AAP_GUI_EXTENSION_URI)) {
        }

        // MIDI
        int32_t getMidiMappingPolicy() override { return midi.getMidiMappingPolicy(); }

        // Parameters
        int32_t getParameterCount() override { return parameters.getParameterCount(); }
        aap_parameter_info_t getParameter(int32_t index) override { return parameters.getParameter(index); }
        double getParameterProperty(int32_t index, int32_t propertyId) override { return parameters.getProperty(index, propertyId); }
        int32_t getEnumerationCount(int32_t index) override { return parameters.getEnumerationCount(index); }
        aap_parameter_enum_t getEnumeration(int32_t index, int32_t enumIndex) override { return parameters.getEnumeration(index, enumIndex); }

        // Presets
        int32_t getPresetCount() override { return presets.getPresetCount(); }
        void getPreset(int32_t index, aap_preset_t& preset) override { presets.getPreset(index, preset); }
        std::string getPresetName(int32_t index) override {
            aap_preset_t preset;
            presets.getPreset(index, preset);
            return preset.name;
        }
        int32_t getCurrentPresetIndex() override { return presets.getPresetIndex(); }
        void setCurrentPresetIndex(int32_t index) override { presets.setPresetIndex(index); }

        // State
        int32_t getStateSize() override { return state.getStateSize(); }
        aap_state_t getState() override {
            aap_state_t stateToSave;
            state.getState(stateToSave);
            return stateToSave;
        }
        void setState(aap_state_t& stateToLoad) override { return state.setState(stateToLoad); }

        // Gui
        aap_gui_instance_id createGui(std::string pluginId, int32_t instanceId, void* audioPluginView) override { return gui.createGui(pluginId, instanceId, audioPluginView); }
        int32_t showGui(aap_gui_instance_id guiInstanceId) override { return gui.showGui(guiInstanceId); }
        int32_t hideGui(aap_gui_instance_id guiInstanceId) override { return gui.hideGui(guiInstanceId); }
        int32_t resizeGui(aap_gui_instance_id guiInstanceId, int32_t width, int32_t height) override { return gui.resizeGui(guiInstanceId, width, height); }
        int32_t destroyGui(aap_gui_instance_id guiInstanceId) override { return gui.destroyGui(guiInstanceId); }
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
        int32_t getMidiMappingPolicy() override { return midi->get_mapping_policy(midi, plugin); }

        // Parameters
        int32_t getParameterCount() override { return parameters->get_parameter_count(parameters, plugin); }
        aap_parameter_info_t getParameter(int32_t index) override { return parameters->get_parameter(parameters, plugin, index); }
        double getParameterProperty(int32_t index, int32_t propertyId) override { return parameters->get_parameter_property(parameters, plugin, index, propertyId); }
        int32_t getEnumerationCount(int32_t index) override { return parameters->get_enumeration_count(parameters, plugin, index); }
        aap_parameter_enum_t getEnumeration(int32_t index, int32_t enumIndex) override { return parameters->get_enumeration(parameters, plugin, index, enumIndex); }

        // Presets
        int32_t getPresetCount() override { return presets->get_preset_count(presets, plugin); }
        void getPreset(int32_t index, aap_preset_t& preset) override { presets->get_preset(presets, plugin, index, &preset, nullptr, nullptr); }
        std::string getPresetName(int32_t index) override {
            aap_preset_t preset;
            return preset.name;
        }
        int32_t getCurrentPresetIndex() override { return presets->get_preset_index(presets, plugin); }
        void setCurrentPresetIndex(int32_t index) override { presets->set_preset_index(presets, plugin, index); }

        // State
        int32_t getStateSize() override { return state->get_state_size(state, plugin); }
        aap_state_t getState() override {
            aap_state_t stateToSave;
            state->get_state(state, plugin, &stateToSave);
            return stateToSave;
        }
        void setState(aap_state_t& stateToLoad) override { state->set_state(state, plugin, &stateToLoad); }

        // Gui
        aap_gui_instance_id createGui(std::string pluginId, int32_t instanceId, void* audioPluginView) override { return gui->create(gui, plugin, pluginId.c_str(), instanceId, audioPluginView); }
        int32_t showGui(aap_gui_instance_id guiInstanceId) override { return gui->show(gui, plugin, guiInstanceId); }
        int32_t hideGui(aap_gui_instance_id guiInstanceId) override { gui->hide(gui, plugin, guiInstanceId); return 0; }
        int32_t resizeGui(aap_gui_instance_id guiInstanceId, int32_t width, int32_t height) override { return gui->resize(gui, plugin, guiInstanceId, width, height); }
        int32_t destroyGui(aap_gui_instance_id guiInstanceId) override { gui->destroy(gui, plugin, guiInstanceId); return 0; }
    };

    class ServiceStandardHostExtensions {
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
        void notifyParametersChanged() { parameters.notifyParametersChanged(); }

        // Presets
        void notifyPresetLoaded() { presets.notifyPresetLoaded(); }
        void notifyPresetsUpdated() { presets.notifyPresetsUpdated(); }

        // State

        // GUI
    };
}

#endif //AAP_CORE_STANDARD_EXTENSIONS_V2_H
