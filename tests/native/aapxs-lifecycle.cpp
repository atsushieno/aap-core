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
constexpr const char* uri = "urn:aap:lifecycle-test";
void check(bool condition, const char* message) {
    if (!condition)
        throw std::runtime_error(message);
}
struct Results {
    int successes{0};
    int errors{0};
    static void completed(void* context, void*) { ++static_cast<Results*>(context)->successes; }
    static void failed(void* context, void*, const char*) { ++static_cast<Results*>(context)->errors; }
};
struct MidiBuffer {
    std::vector<uint32_t> words = std::vector<uint32_t>(20000);
    AAPMidiBufferHeader* header() { return reinterpret_cast<AAPMidiBufferHeader*>(words.data()); }
    static void collect(AAPXSMidi2InitiatorSession* session, void* context, int size) {
        auto self = static_cast<MidiBuffer*>(context);
        auto h = self->header();
        check(size > 0 && sizeof(*h) + h->length + size <= self->words.size() * 4, "encoded MIDI capacity");
        std::memcpy(reinterpret_cast<uint8_t*>(h + 1) + h->length, session->aapxs_rt_midi_buffer, size);
        h->length += size;
    }
};
AAPXSRequestContext makeRequest(AAPXSSerializationContext& data, uint32_t id, Results& result) {
    return {Results::completed, &result, &data, 1, uri, id, 1, Results::failed};
}

void staleToken() {
    int value = 42;
    AAPXSSerializationContext data{&value, 4, 4};
    Results result;
    auto old = makeRequest(data, 7, result);
    AAPXSRequestContext stale{}, fresh{};
    check(sysex8::registerRequest(old, stale), "register old callback");
    check(sysex8::cancelRequest(7, &result, "cancel"), "cancel old callback");
    auto next = makeRequest(data, 1031, result); // same initial table slot
    check(sysex8::registerRequest(next, fresh), "reuse old slot");
    auto errors = result.errors;
    stale.callback(stale.callback_user_data, nullptr);
    stale.error_callback(stale.callback_user_data, nullptr, "late timeout");
    check(result.successes == 0 && result.errors == errors, "stale callback cannot consume new entry");
    check(sysex8::findRequestBuffer(1031) == &data, "fresh entry remains registered");
    fresh.callback(fresh.callback_user_data, nullptr);
    check(result.successes == 1, "fresh callback completes once");
}
struct Fixture {
    AAPXSMidi2InitiatorSession session{8192};
    MidiBuffer replies;
    uint32_t serial{10000};
    int shared{0};
    int replyHandlers{0};
    AAPXSSerializationContext serialization{&shared, 0, 4};
    AAPXSInitiatorInstance initiator{this, nullptr, &serialization, 1, next, send};
    std::unique_ptr<xs::TypedAAPXS> typed = std::make_unique<xs::TypedAAPXS>(uri, &initiator, &serialization);
    std::function<void()> beforeCopy;
    static uint32_t next(AAPXSInitiatorInstance* instance) {
        return static_cast<Fixture*>(instance->aapxs_context)->serial++;
    }
    static bool send(AAPXSInitiatorInstance* instance, AAPXSRequestContext* request) {
        auto self = static_cast<Fixture*>(instance->aapxs_context);
        return AAPXSMidi2SessionAccess::sendRequest(self->session, MidiBuffer::collect, &self->replies, request);
    }
    Fixture() {
        session.setReplyHandler([this](auto* reply) {
            ++replyHandlers;
            if (beforeCopy) beforeCopy();
            auto target = sysex8::findRequestBuffer(reply->request_id);
            check(target && target->data_capacity >= 4, "reply buffer stays registered during copy");
            int value = 42;
            std::memcpy(target->data, &value, 4);
            target->data_size = 4;
        });
    }
    int32_t request(std::function<void(const std::string&, AAPXSSerializationContext*)> callback) {
        // Nonempty payload makes a valid multi-packet request that we reuse as its test reply.
        int value = 0;
        return typed->callFunctionAsync(1, &value, 4, std::move(callback), 4);
    }
};

void cancellationAndDestruction() {
    Fixture fixture;
    int errors = 0;
    for (int i = 0; i < 400; ++i) {
        auto id = fixture.request([&](auto& error, auto*) {
            check(error == "cancelled", "typed cancellation error is preserved");
            ++errors;
        });
        fixture.typed->failAllPending("cancelled");
        check(sysex8::findRequestBuffer(id) == nullptr, "cancellation removes buffer before freeing context");
        fixture.replies.header()->length = 0;
    }
    check(errors == 400, "cancellation reclaims session and global capacity");
    fixture.request([&](auto& error, auto*) {
        check(error == "AAPXS owner destroyed", "typed destructor completes request");
        ++errors;
    });
    auto id = fixture.serial - 1;
    fixture.typed.reset();
    check(errors == 401 && sysex8::findRequestBuffer(id) == nullptr, "destructor removes registrations");
    fixture.session.completeSession(fixture.replies.header(), nullptr);
    check(fixture.replyHandlers == 0, "late replies do not reach freed or cancelled request handlers");

    Results result;
    MidiBuffer midi;
    int value = 42;
    AAPXSSerializationContext data{&value, 4, 4};
    {
        AAPXSMidi2InitiatorSession session(8192);
        auto request = makeRequest(data, 50000, result);
        AAPXSMidi2SessionAccess::sendRequest(session, MidiBuffer::collect, &midi, &request);
    }
    check(result.errors == 1 && sysex8::findRequestBuffer(50000) == nullptr, "session destruction cancels pending request");
}

void cancellationRacesReply() {
    for (int iteration = 0; iteration < 20; ++iteration) {
        Fixture fixture;
        std::promise<void> copying, resume, cancelling;
        auto resumeFuture = resume.get_future().share();
        fixture.beforeCopy = [&] { copying.set_value(); resumeFuture.wait(); };
        int successes = 0, errors = 0;
        auto id = fixture.request([&](auto& error, auto* reply) {
            if (error.empty()) {
                check(*static_cast<int*>(reply->data) == 42, "reply copied before delivery");
                ++successes;
            } else ++errors;
        });
        auto process = std::async(std::launch::async, [&] { fixture.session.completeSession(fixture.replies.header(), nullptr); });
        copying.get_future().wait();
        auto cancel = std::async(std::launch::async, [&] {
            cancelling.set_value();
            fixture.typed->failAllPending("service disconnected");
        });
        cancelling.get_future().wait();
        bool waited = cancel.wait_for(std::chrono::milliseconds(10)) == std::future_status::timeout;
        resume.set_value();
        process.get(); cancel.get();
        check(waited && successes == 1 && errors == 0, "cancellation waits for copy and delivers exactly once");
        check(sysex8::findRequestBuffer(id) == nullptr, "racing completion releases entry");
    }
}

void reentrantAndClosedSession() {
    Fixture fixture;
    int successes = 0, errors = 0;
    fixture.request([&](auto& error, auto*) {
        check(error.empty(), "first reply succeeds");
        ++successes;
        fixture.request([&](auto& nestedError, auto*) {
            check(nestedError == "service disconnected", "reentrant request is cancelled");
            ++errors;
        });
        AAPXSMidi2SessionAccess::cancelPending(fixture.session, "service disconnected", nullptr);
    });
    fixture.session.completeSession(fixture.replies.header(), nullptr);
    check(successes == 1 && errors == 1, "reply callback can enqueue and cancel without deadlock");
    auto length = fixture.replies.header()->length;
    fixture.request([&](auto& error, auto*) {
        check(error == "AAPXS session closed", "closed session reports existing error callback");
        ++errors;
    });
    check(errors == 2 && fixture.replies.header()->length == length, "closed session never encodes more requests");
}

void callbackDestroysClient() {
    Fixture fixture;
    int callbacks = 0;
    auto id = fixture.request([&](auto& error, auto* reply) {
        check(error.empty() && *static_cast<int*>(reply->data) == 42, "self-destroying callback receives reply");
        fixture.typed.reset();
        ++callbacks;
    });
    fixture.session.completeSession(fixture.replies.header(), nullptr);
    check(callbacks == 1 && !fixture.typed && sysex8::findRequestBuffer(id) == nullptr,
          "completion keeps its buffer alive without accessing a destroyed client afterward");
}

void legacyHostRequest() {
    AAPXSMidi2InitiatorSession session(8192);
    MidiBuffer midi;
    int value = 42, hostRequests = 0;
    session.setReplyHandler([&](auto* context) {
        check(context->opcode == -1, "legacy host request retains its opcode");
        ++hostRequests;
    });
    session.addSession(MidiBuffer::collect, &midi, 0, 70000, 1, uri, &value, 4, -1);
    session.completeSession(midi.header(), nullptr);
    check(hostRequests == 1, "unsolicited legacy host requests still reach the handler");
}

int main() {
    try {
        staleToken(); cancellationAndDestruction();
        cancellationRacesReply(); reentrantAndClosedSession(); legacyHostRequest(); callbackDestroysClient();
        std::puts("AAPXS lifecycle tests passed");
    } catch (const std::exception& error) {
        std::fprintf(stderr, "AAPXS lifecycle test failed: %s\n", error.what());
        return 1;
    }
}
