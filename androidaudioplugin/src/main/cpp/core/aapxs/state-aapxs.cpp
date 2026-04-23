#include <algorithm>
#include "aap/core/aapxs/state-aapxs.h"

void aap::xs::AAPXSDefinition_State::aapxs_state_process_incoming_plugin_aapxs_request(
        struct AAPXSDefinition *feature, AAPXSRecipientInstance *aapxsInstance,
        AndroidAudioPlugin *plugin, AAPXSRequestContext *request) {
    auto ext = (aap_state_extension_t*) plugin->get_extension(plugin, AAP_STATE_EXTENSION_URI);
    if (!ext)
        return; // FIXME: should there be any global error handling?
    switch(request->opcode) {
        case OPCODE_GET_STATE_SIZE:
            *((int32_t *) request->serialization->data) = ext->get_state_size(ext, plugin);
            request->serialization->data_size = sizeof(int32_t);
            // RT_SAFE. Send reply now.
            aapxsInstance->send_aapxs_reply(aapxsInstance, request);
            break;
        case OPCODE_GET_STATE: {
            aap_state_t state;
            state.data = request->serialization->data;
            state.data_size = request->serialization->data_capacity;
            ext->get_state(ext, plugin, &state);
            request->serialization->data_size = state.data_size;
            aapxsInstance->send_aapxs_reply(aapxsInstance, request);
            break;
        }
        case OPCODE_SET_STATE: {
            aap_state_t state;
            state.data = request->serialization->data;
            state.data_size = request->serialization->data_size;
            ext->set_state(ext, plugin, &state);
            request->serialization->data_size = 0;
            aapxsInstance->send_aapxs_reply(aapxsInstance, request);
            break;
        }
    }
}

void aap::xs::AAPXSDefinition_State::aapxs_state_process_incoming_host_aapxs_request(
        struct AAPXSDefinition *feature, AAPXSRecipientInstance *aapxsInstance,
        AndroidAudioPluginHost *host, AAPXSRequestContext *request) {
    // there is no host extension!
    throw std::runtime_error("should not happen");
}

void aap::xs::AAPXSDefinition_State::aapxs_state_process_incoming_plugin_aapxs_reply(
        struct AAPXSDefinition *feature, AAPXSInitiatorInstance *aapxsInstance,
        AndroidAudioPlugin *plugin, AAPXSRequestContext *request) {
    if (request->callback != nullptr)
        request->callback(request->callback_user_data, plugin);
}

void aap::xs::AAPXSDefinition_State::aapxs_state_process_incoming_host_aapxs_reply(
        struct AAPXSDefinition *feature, AAPXSInitiatorInstance *aapxsInstance,
        AndroidAudioPluginHost *host, AAPXSRequestContext *request) {
    if (request->callback != nullptr)
        request->callback(request->callback_user_data, host);
}

AAPXSExtensionClientProxy
aap::xs::AAPXSDefinition_State::aapxs_state_get_plugin_proxy(struct AAPXSDefinition *feature,
                                                             AAPXSInitiatorInstance *aapxsInstance,
                                                             AAPXSSerializationContext *serialization) {
    auto client = (AAPXSDefinition_State*) feature->aapxs_context;
    client->typed_client = std::make_unique<StateClientAAPXS>(aapxsInstance, serialization);
    client->client_proxy = AAPXSExtensionClientProxy{client->typed_client.get(), aapxs_parameters_as_plugin_extension};
    return client->client_proxy;
}

size_t aap::xs::StateClientAAPXS::getStateSize() {
    serialization->data_size = 0;
    return callTypedFunctionSynchronously<int32_t>(OPCODE_GET_STATE_SIZE);
}

void aap::xs::StateClientAAPXS::getState(aap_state_t &state) {
    serialization->data_size = 0;
    callVoidFunctionSynchronously(OPCODE_GET_STATE);
    auto copySize = std::min(state.data_size, serialization->data_size);
    if (copySize > 0 && state.data && serialization->data)
        memcpy(state.data, serialization->data, copySize);
    state.data_size = serialization->data_size;
}

void aap::xs::StateClientAAPXS::setState(aap_state_t &state) {
    memcpy(serialization->data, state.data, state.data_size);
    serialization->data_size = state.data_size;
    callVoidFunctionSynchronously(OPCODE_SET_STATE);
}
