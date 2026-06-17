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
            auto serializedData = (uint8_t*) request->serialization->data;
            auto payload = serializedData + sizeof(int32_t);
            auto payloadCapacity = request->serialization->data_capacity - sizeof(int32_t);
            state.data = payload;
            state.data_size = payloadCapacity;
            ext->get_state(ext, plugin, &state);
            auto copySize = std::min(state.data_size, payloadCapacity);
            if (copySize > 0 && state.data != payload)
                memcpy(payload, state.data, copySize);
            *((int32_t*) serializedData) = static_cast<int32_t>(copySize);
            request->serialization->data_size = copySize + sizeof(int32_t);
            if (request->request_id == 0)
                aapxsInstance->send_aapxs_reply(aapxsInstance, request);
            break;
        }
        case OPCODE_SET_STATE: {
            aap_state_t state;
            auto serializedData = (uint8_t*) request->serialization->data;
            state.data_size = *((int32_t*) serializedData);
            state.data = serializedData + sizeof(int32_t);
            ext->set_state(ext, plugin, &state);
            request->serialization->data_size = 0;
            if (request->request_id == 0)
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

std::string aap::xs::StateClientAAPXS::getState(aap_state_t &state) {
    serialization->data_size = 0;
    auto result = callAndWait<int32_t>(OPCODE_GET_STATE, [&state](AAPXSSerializationContext* s) -> int32_t {
        auto serializedData = (uint8_t*) s->data;
        auto actualSize = *((int32_t*) serializedData);
        auto copySize = std::min(state.data_size, static_cast<size_t>(actualSize));
        if (copySize > 0 && state.data && s->data)
            memcpy(state.data, serializedData + sizeof(int32_t), copySize);
        return actualSize;
    });
    if (result.isOk())
        state.data_size = result.value;
    return result.error;
}

std::string aap::xs::StateClientAAPXS::setState(aap_state_t &state) {
    *((int32_t*) serialization->data) = static_cast<int32_t>(state.data_size);
    memcpy((uint8_t*) serialization->data + sizeof(int32_t), state.data, state.data_size);
    serialization->data_size = state.data_size + sizeof(int32_t);
    return callAndWait<bool>(OPCODE_SET_STATE, [](AAPXSSerializationContext*) -> bool { return true; }).error;
}

int32_t aap::xs::StateClientAAPXS::requestStateAsync(std::function<void(Result<aap_state_t>)> callback) {
    serialization->data_size = 0;
    return callFunctionAsync(OPCODE_GET_STATE,
                             [callback = std::move(callback)](const std::string& error, AAPXSSerializationContext* s) {
        if (!callback)
            return;
        if (!error.empty()) {
            callback(Result<aap_state_t>{aap_state_t{nullptr, 0}, error});
            return;
        }
        auto serializedData = (uint8_t*) s->data;
        auto actualSize = *((int32_t*) serializedData);
        // Copy out before the shared block can be reused; the buffer lives for the callback only.
        std::vector<uint8_t> copied(actualSize > 0 ? actualSize : 0);
        if (actualSize > 0)
            memcpy(copied.data(), serializedData + sizeof(int32_t), actualSize);
        aap_state_t result{copied.empty() ? nullptr : copied.data(), static_cast<size_t>(actualSize)};
        callback(Result<aap_state_t>{result, ""});
    });
}

int32_t aap::xs::StateClientAAPXS::setStateAsync(aap_state_t& stateToLoad, std::function<void(Result<bool>)> callback) {
    *((int32_t*) serialization->data) = static_cast<int32_t>(stateToLoad.data_size);
    memcpy((uint8_t*) serialization->data + sizeof(int32_t), stateToLoad.data, stateToLoad.data_size);
    serialization->data_size = stateToLoad.data_size + sizeof(int32_t);
    return callFunctionAsync(OPCODE_SET_STATE,
                             [callback = std::move(callback)](const std::string& error, AAPXSSerializationContext*) {
        if (callback)
            callback(Result<bool>{error.empty(), error});
    });
}
