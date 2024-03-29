
#include "aap/core/aapxs/midi-aapxs.h"
#include "../AAPJniFacade.h"

int32_t getMidiSettingsFromLocalConfig2(std::string pluginId) {
    return aap::AAPJniFacade::getInstance()->getMidiSettingsFromLocalConfig(pluginId);
}

void aap::xs::AAPXSDefinition_Midi::aapxs_midi_process_incoming_plugin_aapxs_request(
        struct AAPXSDefinition *feature, AAPXSRecipientInstance *aapxsInstance,
        AndroidAudioPlugin *plugin, AAPXSRequestContext *request) {
    switch (request->opcode) {
        case OPCODE_GET_MAPPING_POLICY:
            auto len = *(int32_t *) request->serialization->data;
            char *pluginId = nullptr;
            int32_t midiSettings = 0;
            if (len >= AAP_MAX_PLUGIN_ID_SIZE) {
                AAP_ASSERT_FALSE;
            } else {
                pluginId = (char *) calloc(len + 1, 1);
                strncpy(pluginId, (const char *) ((int32_t *) request->serialization->data + 1), len);
                midiSettings = getMidiSettingsFromLocalConfig2(pluginId);
            }
            *((int32_t *) request->serialization->data) = midiSettings;
            aapxsInstance->send_aapxs_reply(aapxsInstance, request);
            if (pluginId)
                free(pluginId);
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
        request->callback(request->callback_user_data, plugin);
}

void aap::xs::AAPXSDefinition_Midi::aapxs_midi_process_incoming_host_aapxs_reply(
        struct AAPXSDefinition *feature, AAPXSInitiatorInstance *aapxsInstance,
        AndroidAudioPluginHost *host, AAPXSRequestContext *request) {
    if (request->callback != nullptr)
        request->callback(request->callback_user_data, host);
}

AAPXSExtensionClientProxy
aap::xs::AAPXSDefinition_Midi::aapxs_midi_get_plugin_proxy(struct AAPXSDefinition *feature,
                                                           AAPXSInitiatorInstance *aapxsInstance,
                                                           AAPXSSerializationContext *serialization) {
    auto client = (AAPXSDefinition_Midi*) feature->aapxs_context;
    if (!client->typed_client)
        client->typed_client = std::make_unique<MidiClientAAPXS>(aapxsInstance, serialization);
    *client->typed_client = MidiClientAAPXS(aapxsInstance, serialization);
    client->client_proxy = AAPXSExtensionClientProxy{client->typed_client.get(), aapxs_midi_as_plugin_extension};
    return client->client_proxy;
}

enum aap_midi_mapping_policy aap::xs::MidiClientAAPXS::getMidiMappingPolicy() {
    return callTypedFunctionSynchronously<enum aap_midi_mapping_policy>(OPCODE_GET_MAPPING_POLICY);
}
