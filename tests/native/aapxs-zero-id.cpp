#include <atomic>
#include <cstdio>
#include <cstring>
#include <future>
#include <memory>
#include <stdexcept>
#include <vector>
#include "aap/core/aapxs/typed-aapxs.h"
#include "aap/ext/midi.h"
#include "aap/core/AAPXSMidi2InitiatorSession.h"
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

int main() {
    AAPXSMidi2InitiatorSession session(8192);
    session.setReplyHandler([](auto*) {});
    MidiBuffer midi;
    Results results;
    int value = 42;
    AAPXSSerializationContext data{&value, 4, 4};
    for (uint32_t id : {0, 1}) {
        auto request = makeRequest(data, id, results);
        AAPXSRequestContext routed{};
        check(sysex8::registerRequest(request, routed), "register IDs zero and one");
        session.addSession(MidiBuffer::collect, &midi, &routed);
    }
    session.completeSession(midi.header(), nullptr);
    check(results.successes == 2 && results.errors == 0, "ID one does not overwrite ID zero");
    session.setRequestTimeoutMs(0);
    midi.header()->length = 0;
    auto request = makeRequest(data, 0, results);
    AAPXSRequestContext routed{};
    check(sysex8::registerRequest(request, routed), "reuse ID zero");
    session.addSession(MidiBuffer::collect, &midi, &routed);
    midi.header()->length = 0;
    session.completeSession(midi.header(), nullptr);
    check(results.errors == 1 && sysex8::findRequestBuffer(0) == nullptr, "ID zero timeout reclaims its slot");
    puts("AAPXS ID-zero regression tests passed");
}
