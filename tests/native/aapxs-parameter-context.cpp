#include <cstdio>
#include <cstring>
#include <functional>
#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>
#include "aap/core/aapxs/parameters-aapxs.h"
#include "aapxs-transport.h"
using aap::xs::ParametersClientAAPXS;
static void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
struct Observer {
    ParametersClientAAPXS* client{};
    void* context{};
    int index{7}, enumeration{2}, calls{0};
    bool success{true};
    std::function<void()> nested;
    std::string error;
};
thread_local Observer* observer;
using ParameterCallback = ParametersClientAAPXS::ParameterCallback;
using EnumerationCallback = ParametersClientAAPXS::EnumerationCallback;
static void parameterReply(ParametersClientAAPXS* client, void* context, int32_t index,
                           aap::Result<aap_parameter_info_t> result) {
    require(client == observer->client && context == observer->context && index == observer->index,
            "parameter callback must preserve client, transport context and index");
    require(result.value.stable_id == (observer->success ? 31 : 0), "parameter callback result");
    require(result.isOk() == observer->success && result.error == observer->error, "explicit result/error status");
    ++observer->calls;
    if (observer->nested) observer->nested();
}
static void enumerationReply(ParametersClientAAPXS* client, void* context, int32_t index,
                             int32_t enumeration, aap::Result<aap_parameter_enum_t> result) {
    require(client == observer->client && context == observer->context && index == observer->index &&
            enumeration == observer->enumeration, "enumeration callback must preserve transport context");
    require(result.value.value == (observer->success ? 2.5 : 0), "enumeration callback result");
    require(result.isOk() == observer->success && result.error == observer->error, "explicit result/error status");
    ++observer->calls;
    if (observer->nested) observer->nested();
}
struct Fixture {
    unsigned char bytes[PARAMETERS_SHARED_MEMORY_SIZE]{};
    AAPXSSerializationContext serialization{bytes, 0, sizeof(bytes)};
    uint32_t nextId{};
    std::vector<AAPXSRequestContext> requests;
    std::function<bool(AAPXSRequestContext*)> send;
    AAPXSInitiatorInstance initiator{this, nullptr, &serialization, 1,
        [](auto* self) { return static_cast<Fixture*>(self->aapxs_context)->nextId++; },
        [](auto* self, auto* request) {
            auto fixture = static_cast<Fixture*>(self->aapxs_context);
            require(request->urid == 1 && strcmp(request->uri, AAP_PARAMETERS_EXTENSION_URI) == 0,
                    "legacy extension identity retained");
            auto index = static_cast<int32_t*>(request->serialization->data);
            require(index[0] == 7, "legacy parameter index payload");
            require(request->serialization->data_size == (request->opcode == OPCODE_PARAMETERS_GET_PARAMETER ? 4u : 8u),
                    "legacy parameter request byte count");
            if (request->opcode == OPCODE_PARAMETERS_GET_ENUMERATION)
                require(index[1] == 2, "legacy enumeration index payload");
            if (fixture->send) return fixture->send(request);
            fixture->requests.push_back(*request); return true;
        }};
    std::unique_ptr<ParametersClientAAPXS> client{
        std::make_unique<ParametersClientAAPXS>(&initiator, &serialization)};
    void parameter() {
        client->getParameterAsync(7, parameterReply);
    }
    void enumeration() {
        client->getEnumerationAsync(7, 2, enumerationReply);
    }
    static void writeReply(AAPXSRequestContext& request) {
        if (request.opcode == OPCODE_PARAMETERS_GET_PARAMETER) {
            aap_parameter_info_t value{}; value.stable_id = 31;
            memcpy(request.serialization->data, &value, sizeof(value));
            request.serialization->data_size = sizeof(value);
        } else {
            aap_parameter_enum_t value{}; value.value = 2.5;
            memcpy(request.serialization->data, &value, sizeof(value));
            request.serialization->data_size = sizeof(value);
        }
    }
};
static void successAndErrors() {
    Fixture f;
    int plugin;
    Observer observation{f.client.get(), &plugin}; observer = &observation;
    aap::xs::AAPXSDefinition_Parameters definition;
    auto& handler = definition.asPublic();
    for (int i = 0; i < 2; ++i) {
        if (i == 0) f.parameter(); else f.enumeration();
        auto& request = f.requests.back();
        require(request.request_id == static_cast<uint32_t>(i), "request IDs retained");
        Fixture::writeReply(request);
        handler.process_incoming_plugin_aapxs_reply(&handler, &f.initiator,
                reinterpret_cast<AndroidAudioPlugin*>(&plugin), &request);
    }
    require(observation.calls == 2, "both actual parameter reply handlers completed");
    f.requests.clear(); observation.success = false; observation.error = "transport failure";
    f.parameter(); f.enumeration();
    for (auto& request : f.requests)
        request.error_callback(request.callback_user_data, &plugin, "transport failure");
    require(observation.calls == 4, "transport errors preserve context and legacy default results");
    f.requests.clear(); observation.context = nullptr; observation.error = "request could not be sent";
    f.send = [](auto*) { return false; };
    f.parameter(); f.enumeration();
    require(observation.calls == 6, "send rejection completes with null context");
}
static void nestedReplyAndFailure() {
    Fixture a, b;
    int pluginA, pluginB;
    Observer outer{a.client.get(), &pluginA}, inner{b.client.get(), &pluginB};
    a.parameter(); b.enumeration();
    outer.nested = [&] {
        observer = &inner;
        Fixture::writeReply(b.requests.back());
        b.requests.back().callback(b.requests.back().callback_user_data, &pluginB);
        // A local failure has no transport context, even inside A's non-null context scope.
        inner.context = nullptr; inner.success = false; inner.error = "nested local failure";
        b.parameter();
        b.client->failAllPending("nested local failure");
        observer = &outer;
    };
    observer = &outer;
    Fixture::writeReply(a.requests.back());
    a.requests.back().callback(a.requests.back().callback_user_data, &pluginA);
    require(outer.calls == 1 && inner.calls == 2, "nested callbacks preserve context isolation");
    // The test transport has no cancellation primitive; failAllPending consumed the call.
    // Do not deliver that abandoned raw transport callback.
}
static void destructionAndImmediateReply() {
    Fixture f;
    int plugin;
    Observer observation{f.client.get(), nullptr}; observation.success = false; observation.error = "AAPXS owner destroyed"; observer = &observation;
    f.parameter(); f.enumeration();
    f.client.reset();
    require(observation.calls == 2, "destruction retains null context/default-result convention");
    for (auto& request : f.requests)
        request.callback(request.callback_user_data, &plugin);
    require(observation.calls == 2, "detached late replies do not deliver again");
    Fixture immediate;
    observation.client = immediate.client.get(); observation.context = &plugin;
    observation.success = true; observation.error.clear();
    observation.nested = [&] { immediate.client.reset(); };
    immediate.send = [&](auto* request) {
        Fixture::writeReply(*request);
        request->callback(request->callback_user_data, &plugin); return true;
    };
    immediate.parameter();
    require(observation.calls == 3 && !immediate.client, "immediate callback can destroy its client");
}
static void parallelContexts() {
    std::exception_ptr failures[2];
    auto work = [&](int index) {
        try {
            for (int i = 0; i < 100; ++i) {
                Fixture f;
                int plugin;
                Observer observation{f.client.get(), &plugin}; observer = &observation;
                f.parameter(); f.enumeration();
                for (auto& request : f.requests) {
                    Fixture::writeReply(request);
                    std::this_thread::yield();
                    request.callback(request.callback_user_data, &plugin);
                }
                require(observation.calls == 2, "parallel context callbacks");
            }
        } catch (...) { failures[index] = std::current_exception(); }
    };
    std::thread first(work, 0), second(work, 1);
    first.join(); second.join();
    for (auto& error : failures) if (error) std::rethrow_exception(error);
}
static void cancellationIdentity() {
    Fixture f;
    Observer observation{f.client.get(), nullptr}; observation.success = false; observation.error = "cancel"; observer = &observation;
    f.send = [&](auto* request) {
        AAPXSRequestContext routed{};
        require(aap::internal::sysex8::registerRequest(*request, routed), "register parameter call");
        f.requests.push_back(routed); return true;
    };
    f.parameter(); f.enumeration();
    f.client->failAllPending("cancel");
    require(observation.calls == 2, "cancellation delivers each parameter call once");
    for (auto& request : f.requests) {
        require(aap::internal::sysex8::findRequestBuffer(request.request_id) == nullptr,
                "forwarding retains original cancellation identity");
        request.callback(request.callback_user_data, nullptr);
    }
    require(observation.calls == 2, "stale cancelled replies do not deliver again");
}
static void shortRepliesAndValidDefaults() {
    Fixture f;
    int plugin;
    Observer observation{f.client.get(), &plugin}; observer = &observation;
    observation.success = false;
    for (int i = 0; i < 2; ++i) {
        observation.error = i ? "short enumeration reply" : "short parameter reply";
        if (i) f.enumeration(); else f.parameter();
        auto request = f.requests.back();
        request.serialization->data_size = 1;
        request.callback(request.callback_user_data, &plugin);
    }
    require(observation.calls == 2, "short replies report errors");
    // The same zero values must be distinguishable as successful data.
    observation.success = true; observation.error.clear();
    f.client->getParameterAsync(7, [](auto*, void*, int32_t, auto result) {
        require(result.isOk() && result.value.stable_id == 0, "valid zero parameter is success");
    });
    auto request = f.requests.back();
    memset(request.serialization->data, 0, request.serialization->data_capacity);
    request.serialization->data_size = sizeof(aap_parameter_info_t);
    request.callback(request.callback_user_data, &plugin);
    f.client->getEnumerationAsync(7, 2, [](auto*, void*, int32_t, int32_t, auto result) {
        require(result.isOk() && result.value.value == 0, "valid zero enumeration is success");
    });
    request = f.requests.back();
    memset(request.serialization->data, 0, request.serialization->data_capacity);
    request.serialization->data_size = sizeof(aap_parameter_enum_t);
    request.callback(request.callback_user_data, &plugin);
}
static void legacyRequestsToCurrentRecipient() {
    aap_parameters_extension_t extension{};
    extension.get_parameter = [](auto*, auto*, int32_t index) {
        require(index == 7, "old client parameter index");
        aap_parameter_info_t result{}; result.stable_id = 31; return result;
    };
    extension.get_enumeration = [](auto*, auto*, int32_t index, int32_t enumeration) {
        require(index == 7 && enumeration == 2, "old client enumeration indices");
        aap_parameter_enum_t result{}; result.value = 2.5; return result;
    };
    AndroidAudioPlugin plugin{}; plugin.plugin_specific = &extension;
    plugin.get_extension = [](auto* p, const char* uri) -> void* {
        require(strcmp(uri, AAP_PARAMETERS_EXTENSION_URI) == 0, "old client extension URI");
        return p->plugin_specific;
    };
    aap::xs::AAPXSDefinition_Parameters definition;
    auto& handler = definition.asPublic();
    alignas(aap_parameter_info_t) unsigned char bytes[PARAMETERS_SHARED_MEMORY_SIZE]{};
    AAPXSSerializationContext serialization{bytes, 4, sizeof(bytes)};
    int replies = 0;
    AAPXSRecipientInstance recipient{&replies, nullptr, &serialization,
        [](auto* self, auto*) { ++*static_cast<int*>(self->aapxs_context); }};
    int32_t payload[]{7, 2};
    memcpy(bytes, payload, 4);
    AAPXSRequestContext oldRequest{nullptr, nullptr, &serialization, 1,
                                  AAP_PARAMETERS_EXTENSION_URI, 42, 2};
    handler.process_incoming_plugin_aapxs_request(&handler, &recipient, &plugin, &oldRequest);
    aap_parameter_info_t parameter{}; memcpy(&parameter, bytes, sizeof(parameter));
    require(parameter.stable_id == 31 && serialization.data_size == sizeof(parameter),
            "new recipient returns the legacy parameter POD record");
    memcpy(bytes, payload, 8); serialization.data_size = 8; oldRequest.opcode = 5;
    handler.process_incoming_plugin_aapxs_request(&handler, &recipient, &plugin, &oldRequest);
    aap_parameter_enum_t enumeration{}; memcpy(&enumeration, bytes, sizeof(enumeration));
    require(enumeration.value == 2.5 && serialization.data_size == sizeof(enumeration) && replies == 2,
            "new recipient returns the legacy enumeration POD record");
}
int main() try {
    successAndErrors(); nestedReplyAndFailure(); destructionAndImmediateReply(); parallelContexts(); cancellationIdentity(); shortRepliesAndValidDefaults(); legacyRequestsToCurrentRecipient();
    puts("AAPXS parameter-context regression tests passed");
} catch (const std::exception& e) {
    fprintf(stderr, "%s\n", e.what()); return 1;
}
