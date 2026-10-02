
#include "aap/core/aapxs/midi-aapxs.h"
#include "../AAPJniFacade.h"
#include "aap/core/host/plugin-instance.h"
#include "midi-policy-payload.h"
#include <array>

int32_t getMidiSettingsFromLocalConfig2(std::string pluginId) {
    return aap::AAPJniFacade::getInstance()->getMidiSettingsFromLocalConfig(pluginId);
}

void aap::xs::AAPXSDefinition_Midi::aapxs_midi_process_incoming_plugin_aapxs_request(
        struct AAPXSDefinition *feature, AAPXSRecipientInstance *aapxsInstance,
        AndroidAudioPlugin *plugin, AAPXSRequestContext *request) {
    switch (request->opcode) {
        case OPCODE_GET_MAPPING_POLICY: {
            auto* data = request->serialization;
            if (!data || !data->data || data->data_capacity < sizeof(int32_t)) {
                if (data)
                    data->data_size = 0;
                aapxsInstance->send_aapxs_reply(aapxsInstance, request);
                break;
            }
            auto* instance = static_cast<aap::PluginInstance*>(aapxsInstance->host_context);
            auto* info = instance ? instance->getPluginInformation() : nullptr;
            auto pluginId = internal::readMidiPolicyPluginId(*data, info ? info->getPluginID() : std::string{});
            int32_t midiSettings = pluginId.empty() ? AAP_PARAMETERS_MAPPING_POLICY_NONE : getMidiSettingsFromLocalConfig2(pluginId);
            memcpy(data->data, &midiSettings, sizeof(midiSettings));
            data->data_size = sizeof(midiSettings);
            aapxsInstance->send_aapxs_reply(aapxsInstance, request);
            break;
        }
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
    client->typed_client = std::make_unique<MidiClientAAPXS>(aapxsInstance, serialization);
    client->client_proxy = AAPXSExtensionClientProxy{client->typed_client.get(), aapxs_midi_as_plugin_extension};
    return client->client_proxy;
}

enum aap_midi_mapping_policy aap::xs::MidiClientAAPXS::getMidiMappingPolicy() {
    auto* instance = static_cast<aap::PluginInstance*>(aapxs_instance->host_context);
    auto* info = instance ? instance->getPluginInformation() : nullptr;
    std::array<char, MIDI_SHARED_MEMORY_SIZE> payload{};
    auto size = internal::writeMidiPolicyPluginId(payload.data(), payload.size(), info ? info->getPluginID() : std::string{});
    if (!size)
        return AAP_PARAMETERS_MAPPING_POLICY_NONE;
    auto result = callAndWait<int32_t>(OPCODE_GET_MAPPING_POLICY, payload.data(), size,
        [](AAPXSSerializationContext* ctx) {
            int32_t policy = AAP_PARAMETERS_MAPPING_POLICY_NONE;
            if (ctx && ctx->data && ctx->data_size >= sizeof(policy) && ctx->data_capacity >= sizeof(policy))
                memcpy(&policy, ctx->data, sizeof(policy));
            return policy;
        }, sizeof(int32_t));
    return result.isOk() ? static_cast<aap_midi_mapping_policy>(result.value) : AAP_PARAMETERS_MAPPING_POLICY_NONE;
}
