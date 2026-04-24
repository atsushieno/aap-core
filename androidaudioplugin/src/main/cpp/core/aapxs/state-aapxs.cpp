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
            auto serializedSize = request->serialization->data_size;
            auto serializedData = (uint8_t*) request->serialization->data;
            if (request->serialization->data_capacity >= sizeof(int32_t)) {
                auto embeddedSize = *((int32_t*) serializedData);
                auto hasEmbeddedSize = embeddedSize >= 0
                        && embeddedSize <= request->serialization->data_capacity - (int32_t) sizeof(int32_t)
                        && (request->request_id == 0
                            || serializedSize == 0
                            || serializedSize == static_cast<size_t>(embeddedSize) + sizeof(int32_t));
                if (hasEmbeddedSize) {
                    state.data = serializedData + sizeof(int32_t);
                    state.data_size = embeddedSize;
                } else {
                    state.data = request->serialization->data;
                    state.data_size = serializedSize;
                }
            } else {
                state.data = request->serialization->data;
                state.data_size = serializedSize;
            }
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
    auto requestedSize = state.data_size;
    callVoidFunctionSynchronously(OPCODE_GET_STATE);
    auto copySize = std::min(state.data_size, serialization->data_size);
    if (serialization->data_size == 0 && requestedSize > 0)
        copySize = requestedSize;
    if (copySize > 0 && state.data && serialization->data)
        memcpy(state.data, serialization->data, copySize);
    state.data_size = serialization->data_size == 0 ? requestedSize : serialization->data_size;
}

void aap::xs::StateClientAAPXS::setState(aap_state_t &state) {
    *((int32_t*) serialization->data) = static_cast<int32_t>(state.data_size);
    memcpy((uint8_t*) serialization->data + sizeof(int32_t), state.data, state.data_size);
    serialization->data_size = state.data_size + sizeof(int32_t);
    callVoidFunctionSynchronously(OPCODE_SET_STATE);
}

aap::xs::StateClientAAPXS::CallbackData*
aap::xs::StateClientAAPXS::allocateCallbackData() {
    for (size_t i = 0; i < UINT8_MAX; i++)
        if (pending_calls[i].context == nullptr)
            return pending_calls + i;
    AAP_ASSERT_FALSE;
    return nullptr;
}

void aap::xs::StateClientAAPXS::completeWithStateCallback(void* callbackData, void* pluginOrHost) {
    auto cb = (CallbackData*) callbackData;
    auto thiz = cb->context;
    auto stateData = std::make_shared<std::vector<uint8_t>>(thiz->serialization->data_size);
    if (!stateData->empty())
        memcpy(stateData->data(), thiz->serialization->data, stateData->size());
    auto callback = std::move(cb->state_callback);
    cb->state_callback = {};
    cb->completion_callback = {};
    cb->context = nullptr;
    if (callback) {
        std::thread([callback = std::move(callback), stateData = std::move(stateData)]() mutable {
            aap_state_t result{stateData->data(), stateData->size()};
            callback(result);
        }).detach();
    }
}

void aap::xs::StateClientAAPXS::completeWithCompletionCallback(void* callbackData, void* pluginOrHost) {
    auto cb = (CallbackData*) callbackData;
    auto callback = std::move(cb->completion_callback);
    cb->state_callback = {};
    cb->completion_callback = {};
    cb->context = nullptr;
    if (callback)
        std::thread([callback = std::move(callback)]() mutable { callback(); }).detach();
}

int32_t aap::xs::StateClientAAPXS::requestStateAsync(std::function<void(aap_state_t)> callback) {
    serialization->data_size = 0;

    auto callbackData = allocateCallbackData();
    if (!callbackData)
        return -1;

    uint32_t requestId = aapxs_instance->get_new_request_id(aapxs_instance);
    *callbackData = {this, std::move(callback), {}};
    AAPXSRequestContext request{completeWithStateCallback, callbackData, serialization,
                                aapxs_instance->urid, AAP_STATE_EXTENSION_URI, requestId,
                                OPCODE_GET_STATE};

    if (!aapxs_instance->send_aapxs_request(aapxs_instance, &request))
        completeWithStateCallback(callbackData, nullptr);

    return requestId;
}

int32_t aap::xs::StateClientAAPXS::setStateAsync(aap_state_t& stateToLoad, std::function<void()> callback) {
    *((int32_t*) serialization->data) = static_cast<int32_t>(stateToLoad.data_size);
    memcpy((uint8_t*) serialization->data + sizeof(int32_t), stateToLoad.data, stateToLoad.data_size);
    serialization->data_size = stateToLoad.data_size + sizeof(int32_t);

    auto callbackData = allocateCallbackData();
    if (!callbackData)
        return -1;

    uint32_t requestId = aapxs_instance->get_new_request_id(aapxs_instance);
    *callbackData = {this, {}, std::move(callback)};
    AAPXSRequestContext request{completeWithCompletionCallback, callbackData, serialization,
                                aapxs_instance->urid, AAP_STATE_EXTENSION_URI, requestId,
                                OPCODE_SET_STATE};

    if (!aapxs_instance->send_aapxs_request(aapxs_instance, &request))
        completeWithCompletionCallback(callbackData, nullptr);

    return requestId;
}
