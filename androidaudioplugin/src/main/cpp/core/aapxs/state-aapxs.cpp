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
    (void) feature;
    (void) serialization;
    return AAPXSExtensionClientProxy{aapxsInstance->aapxs_context, aapxs_parameters_as_plugin_extension};
}

namespace {
    // request: 0..3 size, 4.. data
    std::vector<uint8_t> serializeStateToLoad(const aap_state_t& state) {
        std::vector<uint8_t> payload(sizeof(int32_t) + state.data_size);
        *((int32_t*) payload.data()) = static_cast<int32_t>(state.data_size);
        if (state.data_size > 0)
            memcpy(payload.data() + sizeof(int32_t), state.data, state.data_size);
        return payload;
    }
}

aap::Result<size_t> aap::xs::StateClientAAPXS::getStateSize() {
    auto result = callAndWait<Result<size_t>>(OPCODE_GET_STATE_SIZE, nullptr, 0,
        [](AAPXSSerializationContext* ctx) -> Result<size_t> {
            if (!ctx || !ctx->data || ctx->data_size < sizeof(int32_t) || ctx->data_capacity < sizeof(int32_t))
                return {0, "short state-size reply"};
            int32_t value{};
            memcpy(&value, ctx->data, sizeof(value));
            if (value < 0)
                return {0, "negative state size"};
            return {static_cast<size_t>(value), ""};
        }, sizeof(int32_t));
    return result.isOk() ? std::move(result.value) : Result<size_t>{0, result.error};
}

std::string aap::xs::StateClientAAPXS::getState(aap_state_t &state) {
    auto result = callAndWait<int32_t>(OPCODE_GET_STATE, nullptr, 0, [&state](AAPXSSerializationContext* s) -> int32_t {
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
    if (aap::RealtimeScope::isActive()) return "RT caller";
    auto payload = serializeStateToLoad(state);
    return callAndWait<bool>(OPCODE_SET_STATE, payload.data(), payload.size(), [](AAPXSSerializationContext*) -> bool { return true; }).error;
}

int32_t aap::xs::StateClientAAPXS::requestStateAsync(std::function<void(Result<aap_state_t>)> callback) {
    if (aap::RealtimeScope::isActive()) return -1;
    return callFunctionAsync(OPCODE_GET_STATE, nullptr, 0,
                             [callback = std::move(callback)](const std::string& error, AAPXSSerializationContext* s, void*) {
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
    if (aap::RealtimeScope::isActive()) return -1;
    auto payload = serializeStateToLoad(stateToLoad);
    return callFunctionAsync(OPCODE_SET_STATE, payload.data(), payload.size(),
                             [callback = std::move(callback)](const std::string& error, AAPXSSerializationContext*, void*) {
        if (callback)
            callback(Result<bool>{error.empty(), error});
    });
}
