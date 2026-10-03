#include <atomic>
#include <cstdio>
#include <cstring>
#include <future>
#include <memory>
#include <stdexcept>
#include <vector>
#include "aap/core/aapxs/typed-aapxs.h"
#include "aap/ext/midi.h"
#include "aapxs-midi2-session-internal.h"
#include "aapxs-transport.h"

using namespace aap;
using namespace aap::internal;
using namespace std::chrono_literals;
constexpr const char* uri = "urn:aap:sync-wait-test";
void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
struct Client : xs::TypedAAPXS {
    using TypedAAPXS::TypedAAPXS;
    size_t pendingCount() {
        std::lock_guard<std::mutex> lock(calls_mutex);
        return in_flight.size();
    }
};
struct MidiBuffer {
    std::vector<uint32_t> words = std::vector<uint32_t>(4096);
    AAPMidiBufferHeader* header() { return reinterpret_cast<AAPMidiBufferHeader*>(words.data()); }
    static void collect(AAPXSMidi2InitiatorSession* session, void* context, int32_t size) {
        auto self = static_cast<MidiBuffer*>(context);
        auto h = self->header();
        check(size > 0 && sizeof(*h) + h->length + size <= self->words.size() * 4, "MIDI capture bounds");
        std::memcpy(reinterpret_cast<uint8_t*>(h + 1) + h->length, session->aapxs_rt_midi_buffer, size);
        h->length += size;
    }
};
struct Fixture {
    AAPXSMidi2InitiatorSession session{8192};
    MidiBuffer midi;
    int shared{0};
    uint32_t serial{5000};
    AAPXSSerializationContext data{&shared, 0, 4};
    AAPXSInitiatorInstance initiator{this, nullptr, &data, 1, next, send};
    std::unique_ptr<Client> client = std::make_unique<Client>(uri, &initiator, &data);
    std::function<void(AAPXSRequestContext*)> onSend;
    Fixture() {
        client->setRequestTimeoutMs(20);
        session.setReplyHandler([](auto* reply) {
            auto target = sysex8::findRequestBuffer(reply->request_id);
            check(target != nullptr, "reply target lives until transport completion");
            if (target->data_capacity >= sizeof(int)) {
                int value = 42;
                std::memcpy(target->data, &value, sizeof(value));
                target->data_size = sizeof(value);
            }
        });
    }
    static uint32_t next(AAPXSInitiatorInstance* instance) {
        return static_cast<Fixture*>(instance->aapxs_context)->serial++;
    }
    static bool send(AAPXSInitiatorInstance* instance, AAPXSRequestContext* request) {
        auto self = static_cast<Fixture*>(instance->aapxs_context);
        if (self->onSend) {
            self->onSend(request);
            return true;
        }
        return AAPXSMidi2SessionAccess::sendRequest(self->session, MidiBuffer::collect, &self->midi, request);
    }
    int invoke(bool isVoid) {
        int payload = 7;
        if (isVoid) {
            client->callVoidFunctionSynchronously(1, &payload, sizeof(payload));
            return 0;
        }
        return client->callTypedFunctionSynchronously<int>(1, &payload, sizeof(payload));
    }
};

void noAudioProgress(bool isVoid) {
    Fixture f;
    auto start = std::chrono::steady_clock::now();
    auto waiting = std::async(std::launch::async, [&] { return f.invoke(isVoid); });
    bool returned = waiting.wait_for(1s) == std::future_status::ready;
    if (!returned) f.client->failAllPending("test cleanup"); // Safely fail the old unbounded wrappers.
    int value = waiting.get();
    auto elapsed = std::chrono::steady_clock::now() - start;
    check(returned && elapsed >= 10ms && elapsed < 1s && value == 0, "sync wrapper honours configured reply timeout without audio cycles");
    auto id = f.serial - 1;
    check(f.client->pendingCount() == 1 && sysex8::findRequestBuffer(id) != nullptr,
          "timed-out storage remains owned while the transport holds its address");
    f.session.completeSession(f.midi.header(), nullptr);
    check(f.client->pendingCount() == 0 && sysex8::findRequestBuffer(id) == nullptr,
          "late reply safely releases timed-out call and both registrations");
    f.midi.header()->length = 0;
    f.onSend = [](auto* request) {
        if (request->serialization->data_capacity >= sizeof(int)) {
            int result = 42;
            std::memcpy(request->serialization->data, &result, sizeof(result));
        }
        request->callback(request->callback_user_data, nullptr);
    };
    check(f.invoke(isVoid) == (isVoid ? 0 : 42), "a later successful call still works");
}

void immediateFailureAndDeath(bool isVoid) {
    Fixture f;
    f.onSend = [](auto* request) { request->error_callback(request->callback_user_data, nullptr, "error"); };
    check(f.invoke(isVoid) == 0 && f.client->pendingCount() == 0, "existing error/default behaviour preserved");
    f.onSend = {};
    f.client->setRequestTimeoutMs(1000);
    std::promise<void> emitted;
    f.onSend = [&](auto* request) {
        check(AAPXSMidi2SessionAccess::sendRequest(f.session, MidiBuffer::collect, &f.midi, request), "death fixture send");
        emitted.set_value();
    };
    auto entered = emitted.get_future();
    auto waiting = std::async(std::launch::async, [&] { return f.invoke(isVoid); });
    bool sent = entered.wait_for(1s) == std::future_status::ready;
    f.client->failAllPending("service disconnected");
    check(waiting.get() == 0 && sent && f.client->pendingCount() == 0, "death wakes synchronous wrapper and cancels registration");
}

void repeatedExpiryAndTeardown(bool isVoid) {
    Fixture f;
    f.client->setRequestTimeoutMs(0);
    f.session.setRequestTimeoutMs(0);
    for (int i = 0; i < 300; ++i) {
        check(f.invoke(isVoid) == 0, "immediate configured timeout");
        f.midi.header()->length = 0;
        f.session.completeSession(f.midi.header(), nullptr);
        check(f.client->pendingCount() == 0 && sysex8::findRequestBuffer(f.serial - 1) == nullptr,
              "transport expiry reclaims capacity across more than 255 calls");
    }
    f.invoke(isVoid);
    auto id = f.serial - 1;
    f.client.reset();
    check(sysex8::findRequestBuffer(id) == nullptr, "destruction cancels retained timeout storage");
    f.session.completeSession(f.midi.header(), nullptr); // Late data cannot reach freed storage.
}

void abandonedDeserializerAndActiveDelivery() {
    Fixture f;
    int local = 7, calls = 0;
    auto result = f.client->callAndWait<int>(1, &local, sizeof(local), [&](auto*) { ++calls; return ++local; }, sizeof(int));
    check(result.error == "timeout" && local == 7 && calls == 0, "timeout skips caller-capturing deserializer");
    f.session.completeSession(f.midi.header(), nullptr);
    check(local == 7 && calls == 0 && f.client->pendingCount() == 0, "late completion cannot access caller's abandoned locals");
    f.midi.header()->length = 0;

    // An already-running deserializer may use the caller's locals; its lifetime contract
    // requires waiting for it to finish rather than returning while it still uses those locals.
    std::promise<void> sent, delivering, release;
    auto resume = release.get_future().share();
    auto deliveryEntered = delivering.get_future().share();
    f.onSend = [&](auto* request) {
        AAPXSMidi2SessionAccess::sendRequest(f.session, MidiBuffer::collect, &f.midi, request);
        sent.set_value();
        check(deliveryEntered.wait_for(1s) == std::future_status::ready, "delivery begins before reply-wait timing starts");
    };
    auto entered = sent.get_future();
    auto waiter = std::async(std::launch::async, [&] {
        return f.client->callAndWait<int>(1, &local, sizeof(local), [&](auto*) {
            delivering.set_value(); resume.wait(); return ++local;
        }, sizeof(int));
    });
    bool emitted = entered.wait_for(1s) == std::future_status::ready;
    auto process = std::async(std::launch::async, [&] { f.session.completeSession(f.midi.header(), nullptr); });
    bool began = deliveryEntered.wait_for(1s) == std::future_status::ready;
    bool waited = waiter.wait_for(60ms) == std::future_status::timeout;
    release.set_value(); process.get();
    auto completed = waiter.get();
    check(emitted && began && waited && completed.isOk() && completed.value == 8,
          "active deserialization retains caller lifetime beyond the reply-wait timeout");
}

void binderLateReply() {
    int shared = 0;
    AAPXSSerializationContext data{&shared, 0, sizeof(shared)};
    AAPXSRequestContext routed{};
    auto channel = std::make_shared<AAPXSBinderChannel>(&data, [&](const auto& request) {
        routed = request; return true;
    });
    uint32_t serial = 0;
    struct Context { uint32_t* serial; std::shared_ptr<AAPXSBinderChannel> channel; } context{&serial, channel};
    AAPXSInitiatorInstance initiator{&context, nullptr, &data, 1,
        [](auto* self) { return ++*static_cast<Context*>(self->aapxs_context)->serial; },
        [](auto* self, auto* request) { return static_cast<Context*>(self->aapxs_context)->channel->send(request); }};
    Client client(uri, &initiator, &data);
    client.setRequestTimeoutMs(10);
    check(client.callTypedFunctionSynchronously<int>(1, nullptr, 0) == 0 && client.pendingCount() == 1,
          "Binder timeout keeps request buffer owned until its callback");
    shared = 42;
    routed.callback(routed.callback_user_data, nullptr);
    check(client.pendingCount() == 0, "late Binder copy and completion remain safe");
}

void completionDestroysClient() {
    Fixture f;
    f.onSend = [&](auto* request) {
        int value = 42;
        std::memcpy(request->serialization->data, &value, sizeof(value));
        request->callback(request->callback_user_data, nullptr);
        f.client.reset();
    };
    check(f.invoke(false) == 42 && !f.client, "synchronous completion may destroy the client without a later member read");
}

int main() try {
    for (bool isVoid : {false, true}) {
        noAudioProgress(isVoid);
        immediateFailureAndDeath(isVoid);
        repeatedExpiryAndTeardown(isVoid);
    }
    abandonedDeserializerAndActiveDelivery();
    binderLateReply();
    completionDestroysClient();
    puts("AAPXS synchronous reply-wait regressions passed");
}
catch (const std::exception& error) {
    std::fprintf(stderr, "%s\n", error.what());
    return 1;
}
