#include <atomic>
#include <cstdio>
#include <cstring>
#include <future>
#include <stdexcept>
#include <vector>
#include "aapxs-midi2-session-internal.h"
#include "aapxs-transport.h"
#include "instance-realtime-state.h"
#include "aap/core/realtime.h"

using namespace aap;
using namespace aap::internal;
void check(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
constexpr const char* uri = "urn:aap:rt-handoff-test";
struct Result {
    std::atomic<int> successes{0}, errors{0};
    static void complete(void* context, void*) {
        check(!RealtimeScope::isActive(), "completion stays off processing");
        ++static_cast<Result*>(context)->successes;
    }
    static void fail(void* context, void*, const char*) { ++static_cast<Result*>(context)->errors; }
};
void handoffAndLateCancellation() {
    InstanceRealtimeState state(8192);
    AAPXSMidi2InitiatorSession session(8192);
    uint32_t value = 42;
    AAPXSSerializationContext data{&value, 4, 4};
    Result result;
    AAPXSRequestContext request{Result::complete, &result, &data, 1, uri, 1, 1, Result::fail};
    std::vector<uint32_t> wire(2048);
    auto* header = reinterpret_cast<AAPMidiBufferHeader*>(wire.data());
    check(AAPXSMidi2SessionAccess::sendRequest(session, [](auto* session, void* data, int32_t size) {
        auto* header = static_cast<AAPMidiBufferHeader*>(data);
        header->length = size;
        memcpy(header + 1, session->aapxs_rt_midi_buffer, size);
        return true;
    }, header, &request), "request registered");
    // Reuse the existing wire request as an old peer's reply.
    uint32_t note[2]{0x40903C00, 0x7FFFFFFF};
    memcpy(reinterpret_cast<uint8_t*>(header + 1) + header->length, note, sizeof(note));
    header->length += sizeof(note);
    {
        RealtimeScope rt;
        sysex8::filterOutMessages(header, &state, queueAAPXSMidi2Input);
        check(result.successes == 0, "filtering never calls completion");
        check(header->length == sizeof(note) && !memcmp(header + 1, note, sizeof(note)), "ordinary MIDI preserved");
    }
    // Audio may reuse its buffer immediately; the worker owns a copy.
    memset(wire.data(), 0, wire.size() * sizeof(uint32_t));
    check(state.aapxs_input.tryConsume([&](void* data, size_t) {
        session.completeSession(data, nullptr);
        return true;
    }), "worker receives copied reply");
    check(result.successes == 1 && result.errors == 0, "reply delivered once");

    request.request_id = 2;
    AAPXSMidi2SessionAccess::sendRequest(session, [](auto* session, void* data, int32_t size) {
        auto* header = static_cast<AAPMidiBufferHeader*>(data);
        header->length = size;
        memcpy(header + 1, session->aapxs_rt_midi_buffer, size);
        return true;
    }, header, &request);
    { RealtimeScope rt; sysex8::filterOutMessages(header, &state, queueAAPXSMidi2Input); }
    check(sysex8::cancelRequest(2, &result, "cancel"), "cancel before queued reply delivery");
    data.data = nullptr; // a released buffer must not be reached by the retained wire copy
    state.aapxs_input.tryConsume([&](void* data, size_t) { session.completeSession(data, nullptr); return true; });
    check(result.successes == 1 && result.errors == 1, "late copied reply ignored after cancellation");
}
void outgoingBackpressure() {
    AAPXSMidi2InitiatorSession session(8192);
    uint32_t value = 42;
    AAPXSSerializationContext data{&value, 4, 4};
    Result result;
    AAPXSRequestContext request{Result::complete, &result, &data, 1, uri, 3, 1, Result::fail};
    check(AAPXSMidi2SessionAccess::sendRequest(session, [](auto*, void*, int32_t) { return false; }, nullptr, &request),
          "rejection completes accepted transport request");
    check(result.errors == 1 && sysex8::findRequestBuffer(3) == nullptr, "full handoff releases pending ownership");
    AAPMidiBufferHeader empty{};
    session.setRequestTimeoutMs(0);
    session.completeSession(&empty, nullptr);
    check(result.errors == 1, "rejected entry cannot time out again");
}
int main() {
    handoffAndLateCancellation(); outgoingBackpressure();
    puts("PASS: copied wire reply handoff, late cancellation and queue-full ownership");
}
