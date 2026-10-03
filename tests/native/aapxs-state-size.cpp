#include <cstdio>
#include <cstring>
#include <future>
#include <memory>
#include <stdexcept>
#include "aap/core/aapxs/state-aapxs.h"
#include "aap/core/aapxs/standard-extensions.h"
static void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
struct Fixture {
    int32_t buffer{};
    AAPXSSerializationContext serialization{&buffer, 0, sizeof(buffer)};
    uint32_t id{};
    std::function<bool(AAPXSRequestContext*)> send;
    AAPXSInitiatorInstance initiator{this, nullptr, &serialization, 1,
        [](auto* self) { return static_cast<Fixture*>(self->aapxs_context)->id++; },
        [](auto* self, auto* request) {
            require(request->opcode == OPCODE_GET_STATE_SIZE && request->serialization->data_size == 0,
                    "legacy state-size request: opcode 1, no payload");
            return static_cast<Fixture*>(self->aapxs_context)->send(request);
        }};
    aap::xs::StateClientAAPXS client{&initiator, &serialization};
    void reply(int32_t value, size_t bytes = sizeof(int32_t)) {
        send = [=](auto* request) {
            memcpy(request->serialization->data, &value, sizeof(value));
            request->serialization->data_size = bytes;
            request->callback(request->callback_user_data, nullptr); return true;
        };
    }
};
static void resultsAndLegacyBridge() {
    Fixture f;
    f.reply(0);
    auto zero = f.client.getStateSize();
    require(zero.isOk() && zero.value == 0, "zero state size is successful");
    f.reply(123);
    auto positive = f.client.getStateSize();
    require(positive.isOk() && positive.value == 123, "legacy int32 reply decodes as size_t");
    f.reply(-1);
    auto negative = f.client.getStateSize();
    require(!negative.isOk() && negative.error == "negative state size", "reject negative state size");
    f.reply(12, 1);
    auto shortReply = f.client.getStateSize();
    require(!shortReply.isOk() && shortReply.error == "short state-size reply", "short state size reports error");
    f.send = [](auto* request) {
        request->error_callback(request->callback_user_data, nullptr, "service disconnected"); return true;
    };
    auto failure = f.client.getStateSize();
    require(!failure.isOk() && failure.value == 0 && failure.error == "service disconnected",
            "transport error stays distinct from valid zero");
    // Older plugin/host C extension clients retain their original value-only ABI and defaults.
    auto extension = f.client.asPluginExtension();
    require(extension->get_state_size(extension, nullptr) == 0, "legacy C bridge retains error default");
    f.reply(123);
    require(extension->get_state_size(extension, nullptr) == 123, "legacy C bridge retains successful size");
    f.send = [](auto*) { return false; };
    require(f.client.getStateSize().error == "request could not be sent", "send rejection retained");
}
static void timeoutAndDeath() {
    Fixture f;
    AAPXSRequestContext pending{};
    f.client.setRequestTimeoutMs(10);
    f.send = [&](auto* request) { pending = *request; return true; };
    auto timeout = f.client.getStateSize();
    require(timeout.error == "timeout", "state-size wait timeout is explicit");
    int32_t value = 123;
    memcpy(pending.serialization->data, &value, sizeof(value));
    pending.callback(pending.callback_user_data, nullptr); // abandoned deserializer is skipped
    std::promise<void> entered;
    f.client.setRequestTimeoutMs(1000);
    f.send = [&](auto* request) { pending = *request; entered.set_value(); return true; };
    auto waiting = std::async(std::launch::async, [&] { return f.client.getStateSize(); });
    entered.get_future().wait();
    f.client.failAllPending("service disconnected");
    require(waiting.get().error == "service disconnected", "death wakes state-size caller with error");
}
static void localExtensionResults() {
    aap_state_extension_t state{nullptr,
        [](aap_state_extension_t*, AndroidAudioPlugin*) -> size_t { return 0; }, nullptr, nullptr};
    AndroidAudioPlugin plugin{};
    plugin.plugin_specific = &state;
    plugin.get_extension = [](AndroidAudioPlugin* p, const char* uri) -> void* {
        return strcmp(uri, AAP_STATE_EXTENSION_URI) == 0 ? p->plugin_specific : nullptr;
    };
    aap::xs::ServiceStandardExtensions available{&plugin};
    auto size = available.getStateSize();
    require(size.isOk() && size.value == 0, "local legitimate empty state retains success");
    plugin.plugin_specific = nullptr;
    aap::xs::ServiceStandardExtensions absent{&plugin};
    require(!absent.getStateSize().isOk() && !absent.getState().isOk(), "missing local state extension reports error");
}
static void legacyRequestToCurrentRecipient() {
    aap_state_extension_t extension{};
    extension.get_state_size = [](auto*, auto*) -> size_t { return 123; };
    AndroidAudioPlugin plugin{}; plugin.plugin_specific = &extension;
    plugin.get_extension = [](auto* p, const char* uri) -> void* {
        require(strcmp(uri, AAP_STATE_EXTENSION_URI) == 0, "old state client extension URI");
        return p->plugin_specific;
    };
    aap::xs::AAPXSDefinition_State definition;
    auto& handler = definition.asPublic();
    int32_t size{};
    AAPXSSerializationContext serialization{&size, 0, sizeof(size)};
    int replies = 0;
    AAPXSRecipientInstance recipient{&replies, nullptr, &serialization,
        [](auto* self, auto*) { ++*static_cast<int*>(self->aapxs_context); }};
    AAPXSRequestContext oldRequest{nullptr, nullptr, &serialization, 1, AAP_STATE_EXTENSION_URI, 42, 1};
    handler.process_incoming_plugin_aapxs_request(&handler, &recipient, &plugin, &oldRequest);
    require(size == 123 && serialization.data_size == 4 && replies == 1,
            "new recipient returns the legacy four-byte state-size response");
}
int main() try {
    resultsAndLegacyBridge(); timeoutAndDeath(); localExtensionResults(); legacyRequestToCurrentRecipient();
    puts("AAPXS state-size result regressions passed");
} catch (const std::exception& e) {
    fprintf(stderr, "%s\n", e.what()); return 1;
}
