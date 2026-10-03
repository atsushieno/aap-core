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
};
thread_local Observer* observer;
using ParameterCallback = void (*)(ParametersClientAAPXS*, void*, int32_t, aap_parameter_info_t);
using EnumerationCallback = void (*)(ParametersClientAAPXS*, void*, int32_t, int32_t, aap_parameter_enum_t);
static void parameterReply(ParametersClientAAPXS* client, void* context, int32_t index,
                           aap_parameter_info_t result) {
    require(client == observer->client && context == observer->context && index == observer->index,
            "parameter callback must preserve client, transport context and index");
    require(result.stable_id == (observer->success ? 31 : 0), "parameter callback result");
    ++observer->calls;
    if (observer->nested) observer->nested();
}
static void enumerationReply(ParametersClientAAPXS* client, void* context, int32_t index,
                             int32_t enumeration, aap_parameter_enum_t result) {
    require(client == observer->client && context == observer->context && index == observer->index &&
            enumeration == observer->enumeration, "enumeration callback must preserve transport context");
    require(result.value == (observer->success ? 2.5 : 0), "enumeration callback result");
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
            if (fixture->send) return fixture->send(request);
            fixture->requests.push_back(*request); return true;
        }};
    std::unique_ptr<ParametersClientAAPXS> client{
        std::make_unique<ParametersClientAAPXS>(&initiator, &serialization)};
    void parameter() {
        // Preserve the legacy encoded function-pointer argument. Changing this odd public
        // pointer-to-function-pointer convention is outside the context-forwarding fix.
        client->getParameterAsync(7, reinterpret_cast<ParameterCallback*>(parameterReply));
    }
    void enumeration() {
        client->getEnumerationAsync(7, 2, reinterpret_cast<EnumerationCallback*>(enumerationReply));
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
    f.requests.clear(); observation.success = false;
    f.parameter(); f.enumeration();
    for (auto& request : f.requests)
        request.error_callback(request.callback_user_data, &plugin, "transport failure");
    require(observation.calls == 4, "transport errors preserve context and legacy default results");
    f.requests.clear(); observation.context = nullptr;
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
        inner.context = nullptr; inner.success = false;
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
    Observer observation{f.client.get(), nullptr}; observation.success = false; observer = &observation;
    f.parameter(); f.enumeration();
    f.client.reset();
    require(observation.calls == 2, "destruction retains null context/default-result convention");
    for (auto& request : f.requests)
        request.callback(request.callback_user_data, &plugin);
    require(observation.calls == 2, "detached late replies do not deliver again");
    Fixture immediate;
    observation.client = immediate.client.get(); observation.context = &plugin;
    observation.success = true;
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
    Observer observation{f.client.get(), nullptr}; observation.success = false; observer = &observation;
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
int main() try {
    successAndErrors(); nestedReplyAndFailure(); destructionAndImmediateReply(); parallelContexts(); cancellationIdentity();
    puts("AAPXS parameter-context regression tests passed");
} catch (const std::exception& e) {
    fprintf(stderr, "%s\n", e.what()); return 1;
}
