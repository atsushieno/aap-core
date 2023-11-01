
#include "state-aapxs.h"

void aap::xs::AAPXSDefinition_State::aapxs_state_process_incoming_plugin_aapxs_request(
        struct AAPXSDefinition *feature, AAPXSRecipientInstance *aapxsInstance,
        AndroidAudioPlugin *plugin, AAPXSRequestContext *request) {
    auto ext = (aap_state_extension_t*) plugin->get_extension(plugin, AAP_STATE_EXTENSION_URI);
    switch(request->opcode) {
        case OPCODE_GET_STATE_SIZE:
            *((int32_t *) request->serialization->data) = ext->get_state_size(ext, plugin);
            // RT_SAFE. Send reply now.
            aapxsInstance->send_aapxs_reply(aapxsInstance, request);
            break;
        case OPCODE_GET_STATE: {
            // FIXME: this is not RT-safe, should be processed asynchronously
            aap_state_t state;
            state.data = request->serialization->data;
            ext->get_state(ext, plugin, &state);
            aapxsInstance->send_aapxs_reply(aapxsInstance, request);
            break;
        }
        case OPCODE_SET_STATE: {
            // FIXME: this is not RT-safe, should be processed asynchronously
            aap_state_t state;
            state.data = request->serialization->data;
            ext->set_state(ext, plugin, &state);
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
        request->callback(request->callback_user_data, plugin, request->request_id);
}

void aap::xs::AAPXSDefinition_State::aapxs_state_process_incoming_host_aapxs_reply(
        struct AAPXSDefinition *feature, AAPXSInitiatorInstance *aapxsInstance,
        AndroidAudioPluginHost *host, AAPXSRequestContext *request) {
    if (request->callback != nullptr)
        request->callback(request->callback_user_data, host, request->request_id);
}

int32_t aap::xs::StateClientAAPXS::getStateSize() {
    return callTypedFunctionSynchronously<int32_t>(OPCODE_GET_STATE_SIZE);
}

void aap::xs::StateClientAAPXS::getState(aap_state_t &state) {
    callVoidFunctionSynchronously(OPCODE_GET_STATE);
    memcpy(state.data, serialization->data, state.data_size);
}

void aap::xs::StateClientAAPXS::setState(aap_state_t &state) {
    memcpy(serialization->data, state.data, state.data_size);
    callVoidFunctionSynchronously(OPCODE_SET_STATE);
}
