
#include "midi-aapxs.h"
#include "../AAPJniFacade.h"

#if USE_AAPXS_V2
int32_t getMidiSettingsFromLocalConfig(std::string pluginId) {
    return aap::AAPJniFacade::getInstance()->getMidiSettingsFromLocalConfig(pluginId);
}
#else
extern int32_t getMidiSettingsFromLocalConfig(std::string pluginId);
#endif

void aap::xs::AAPXSDefinition_Midi::aapxs_midi_process_incoming_plugin_aapxs_request(
        struct AAPXSDefinition *feature, AAPXSRecipientInstance *aapxsInstance,
        AndroidAudioPlugin *plugin, AAPXSRequestContext *request) {
    switch (request->opcode) {
        case OPCODE_GET_MAPPING_POLICY:
            auto len = *(int32_t *) request->serialization->data;
            assert(len < AAP_MAX_PLUGIN_ID_SIZE);
            char *pluginId = (char *) calloc(len + 1, 1);
            strncpy(pluginId, (const char *) ((int32_t *) request->serialization->data + 1), len);
            *((int32_t *) request->serialization->data) = getMidiSettingsFromLocalConfig(pluginId);
            break;
    }
}

void aap::xs::AAPXSDefinition_Midi::aapxs_midi_process_incoming_host_aapxs_request(
        struct AAPXSDefinition *feature, AAPXSRecipientInstance *aapxsInstance,
        AndroidAudioPluginHost *host, AAPXSRequestContext *request) {
    throw std::runtime_error("There is no MIDI host extension");
}

void aap::xs::AAPXSDefinition_Midi::aapxs_midi_process_incoming_plugin_aapxs_reply(
        struct AAPXSDefinition *feature, AAPXSInitiatorInstance *aapxsInstance,
        AndroidAudioPlugin *plugin, AAPXSRequestContext *request) {
    if (request->callback != nullptr)
        request->callback(request->callback_user_data, plugin, request->request_id);
}

void aap::xs::AAPXSDefinition_Midi::aapxs_midi_process_incoming_host_aapxs_reply(
        struct AAPXSDefinition *feature, AAPXSInitiatorInstance *aapxsInstance,
        AndroidAudioPluginHost *host, AAPXSRequestContext *request) {
    if (request->callback != nullptr)
        request->callback(request->callback_user_data, host, request->request_id);
}

int32_t aap::xs::MidiClientAAPXS::getMidiMappingPolicy() {
    return callTypedFunctionSynchronously<int32_t>(OPCODE_GET_MAPPING_POLICY);
}
