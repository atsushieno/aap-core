#include <atomic>
#include <cstdio>
#include <cstring>
#include <future>
#include <memory>
#include <stdexcept>
#include <vector>
#include "aap/ext/midi.h"
#include "aapxs-midi2-session-internal.h"
#include "aapxs-transport.h"

using namespace aap;
using namespace aap::internal;
using namespace std::chrono_literals;
constexpr const char* uri = "urn:aap:session-isolation-test";
void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
struct Results {
    int successes{0}, errors{0};
    static void completed(void* context, void*) { ++static_cast<Results*>(context)->successes; }
    static void failed(void* context, void*, const char*) { ++static_cast<Results*>(context)->errors; }
};
struct MidiBuffer {
    std::vector<uint32_t> words = std::vector<uint32_t>(4096);
    AAPMidiBufferHeader* header() { return reinterpret_cast<AAPMidiBufferHeader*>(words.data()); }
    static bool collect(AAPXSMidi2InitiatorSession* session, void* context, int32_t size) {
        auto self = static_cast<MidiBuffer*>(context);
        auto h = self->header();
        check(size > 0 && sizeof(*h) + h->length + size <= self->words.size() * 4, "MIDI capture bounds");
        std::memcpy(reinterpret_cast<uint8_t*>(h + 1) + h->length, session->aapxs_rt_midi_buffer, size);
        h->length += size;
        return true;
    }
};
struct BlockingEmission {
    std::promise<void> entered, release;
    std::shared_future<void> resume = release.get_future().share();
    static bool emit(AAPXSMidi2InitiatorSession*, void* context, int32_t) {
        auto self = static_cast<BlockingEmission*>(context);
        self->entered.set_value();
        self->resume.wait();
        return true;
    }
};

// Hold one session inside its transport callback. Every tested operation on another session
// must finish before the blocked callback is released, not merely finish eventually.
void independentSession(bool rawSender, int operation) {
    AAPXSMidi2InitiatorSession blocked(8192);
    auto other = std::make_unique<AAPXSMidi2InitiatorSession>(8192);
    other->setReplyHandler([](auto*) {});
    int value = 42;
    AAPXSSerializationContext data{&value, 4, 4};
    Results result;
    MidiBuffer midi;
    AAPXSRequestContext request{Results::completed, &result, &data, 1, uri, 123, 1, Results::failed};
    if (operation >= 2) {
        other->setRequestTimeoutMs(operation == 3 ? 0 : 1000);
        check(AAPXSMidi2SessionAccess::sendRequest(*other, MidiBuffer::collect, &midi, &request), "prepare other session");
    }
    BlockingEmission emission;
    auto entered = emission.entered.get_future();
    auto send = std::async(std::launch::async, [&] {
        if (rawSender)
            blocked.addSession(BlockingEmission::emit, &emission, 0, 456, 1, uri, &value, 4, 1);
        else {
            AAPXSRequestContext notification{nullptr, nullptr, &data, 1, uri, 456, 1, nullptr};
            blocked.addSession(BlockingEmission::emit, &emission, &notification);
        }
    });
    bool started = entered.wait_for(1s) == std::future_status::ready;
    auto progress = std::async(std::launch::async, [&] {
        switch (operation) {
            case 0:
                check(AAPXSMidi2SessionAccess::sendRequest(*other, MidiBuffer::collect, &midi, &request), "independent send");
                other->completeSession(midi.header(), nullptr);
                break;
            case 1:
                other->addSession(MidiBuffer::collect, &midi, 0, 789, 1, uri, &value, 4, 1);
                break;
            case 2:
                other->completeSession(midi.header(), nullptr);
                break;
            case 3:
                midi.header()->length = 0;
                other->completeSession(midi.header(), nullptr);
                break;
            case 4:
                check(sysex8::cancelRequest(123, &result, "cancelled"), "independent cancellation");
                break;
            case 5:
                other.reset();
                break;
        }
    });
    bool independent = progress.wait_for(1s) == std::future_status::ready;
    emission.release.set_value(); // Always unblock before checking, including on the old code.
    send.get(); progress.get();
    check(started && independent, "one session's stalled sender blocked another session");
    if (operation == 0 || operation == 2)
        check(result.successes == 1 && result.errors == 0, "independent reply completion");
    if (operation >= 3)
        check(result.successes == 0 && result.errors == 1, "independent timeout/cancellation/destruction");
    check(sysex8::findRequestBuffer(123) == nullptr, "other session releases its pending registration");
}

void sameSessionStillSerializes() {
    AAPXSMidi2InitiatorSession session(8192);
    int value = 42;
    MidiBuffer midi;
    BlockingEmission emission;
    auto entered = emission.entered.get_future();
    auto first = std::async(std::launch::async, [&] {
        session.addSession(BlockingEmission::emit, &emission, 0, 1, 1, uri, &value, 4, 1);
    });
    bool started = entered.wait_for(1s) == std::future_status::ready;
    std::promise<void> trying;
    auto second = std::async(std::launch::async, [&] {
        trying.set_value();
        session.addSession(MidiBuffer::collect, &midi, 0, 2, 1, uri, &value, 4, 1);
    });
    trying.get_future().wait();
    bool waited = second.wait_for(20ms) == std::future_status::timeout;
    emission.release.set_value();
    first.get(); second.get();
    check(started && waited && midi.header()->length > 0, "same-session buffer access is serialized");
}

void synchronousReplyFromSend() {
    AAPXSMidi2InitiatorSession session(8192);
    session.setReplyHandler([](auto*) {});
    MidiBuffer midi;
    int value = 42;
    AAPXSSerializationContext data{&value, 4, 4};
    Results result;
    AAPXSRequestContext request{Results::completed, &result, &data, 1, uri, 123, 1, Results::failed};
    check(AAPXSMidi2SessionAccess::sendRequest(session, [](auto* owner, void* context, int32_t size) {
        MidiBuffer::collect(owner, context, size);
        owner->completeSession(static_cast<MidiBuffer*>(context)->header(), nullptr);
        return true;
    }, &midi, &request), "reentrant send contract");
    check(result.successes == 1 && result.errors == 0 && sysex8::findRequestBuffer(123) == nullptr,
          "send callback may deliver a reply reentrantly");
}

int main() try {
    for (bool raw : {false, true})
        for (int operation = 0; operation < 6; ++operation)
            independentSession(raw, operation);
    sameSessionStillSerializes();
    synchronousReplyFromSend();
    puts("AAPXS session-isolation regressions passed");
}
catch (const std::exception& error) {
    std::fprintf(stderr, "%s\n", error.what());
    return 1;
}
