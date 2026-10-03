#include <atomic>
#include <chrono>
#include <cstdio>
#include <stdexcept>
#include <thread>
#include <vector>
#include "aap/core/realtime.h"
#include "aap/ext/midi.h"
#include "aapxs-midi2-session-internal.h"
#include "instance-extension-worker.h"
#include "realtime-byte-queue.h"

using namespace aap::internal;
using Clock = EventWakeup::Clock;
void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
template<class Predicate> void await(Predicate&& predicate, const char* message) {
    auto limit = Clock::now() + std::chrono::seconds(2);
    while (!predicate()) {
        check(Clock::now() < limit, message);
        std::this_thread::yield();
    }
}
void idleAndSleepRaces() {
    InstanceExtensionWorker worker;
    std::atomic<unsigned> published{0}, delivered{0}, dispatches{0};
    worker.start([&] { delivered.store(published.load()); ++dispatches; });
    await([&] { return dispatches.load() != 0; }, "worker starts");
    auto initial = dispatches.load();
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    check(dispatches == initial, "idle worker does not poll");
    for (unsigned n = 1; n <= 5000; ++n) {
        {
            aap::RealtimeScope rt;
            published.store(n, std::memory_order_release);
            worker.notify();
        }
        await([&] { return delivered.load() == n; }, "publication racing with worker sleep is delivered");
    }
    auto begin = Clock::now();
    worker.stop();
    check(Clock::now() - begin < std::chrono::seconds(1), "shutdown wakes idle worker");
}
void concurrentQueueWakeups() {
    RealtimeByteQueue<64> queue(sizeof(uint32_t));
    InstanceExtensionWorker worker;
    std::atomic<unsigned> accepted{0}, consumed{0};
    worker.start([&] {
        for (unsigned n = 0; n < 64; ++n)
            if (!queue.tryConsume([&](void*, size_t) { ++consumed; return true; })) break;
    });
    std::vector<std::thread> producers;
    for (unsigned i = 0; i < 4; ++i)
        producers.emplace_back([&] {
            for (uint32_t n = 0; n < 20000; ++n) {
                aap::RealtimeScope rt;
                if (queue.tryPush(&n, sizeof(n))) { ++accepted; worker.notify(); }
            }
        });
    for (auto& producer : producers) producer.join();
    await([&] { return consumed == accepted; }, "coalesced wakeups drain every accepted record");
    worker.stop();
    check(accepted + queue.rejectedCount() == 80000, "all publications accounted for");
}
void sessionDeadlines() {
    aap::AAPXSMidi2InitiatorSession session(8192);
    InstanceExtensionWorker worker;
    std::atomic<unsigned> dispatches{0};
    std::atomic<bool> idle{true};
    AAPXSMidi2SessionAccess::setDeadlineChangedHandler(session, [&] { worker.notify(); });
    worker.start([&] {
        AAPMidiBufferHeader empty{};
        session.completeSession(&empty, nullptr);
        ++dispatches;
    }, [&] {
        auto next = AAPXSMidi2SessionAccess::nextDeadline(session);
        idle.store(next == EventWakeup::Deadline::max());
        return next;
    });
    struct Completion { std::atomic<bool> expired{false}; } longer, shorter;
    uint32_t payload = 0;
    AAPXSSerializationContext serialization{&payload, sizeof(payload), sizeof(payload)};
    auto makeRequest = [&](Completion& result, uint32_t id) {
        AAPXSRequestContext request{
            [](void*, void*) { check(false, "timeout must use error callback"); }, &result,
            &serialization, 1, "urn:aap:event-worker", id, 1};
        request.error_callback = [](void* context, void*, const char* error) {
            check(std::string(error) == "timeout", "deadline completion reports timeout");
            static_cast<Completion*>(context)->expired = true;
        };
        return request;
    };
    auto add = [](auto*, void*, int32_t) { return true; };
    session.setRequestTimeoutMs(3000);
    auto first = makeRequest(longer, 1);
    session.addSession(add, nullptr, &first);
    await([&] { return !idle.load(); }, "registration wakes worker from indefinite wait");
    session.setRequestTimeoutMs(25);
    auto second = makeRequest(shorter, 2);
    session.addSession(add, nullptr, &second);
    await([&] { return shorter.expired.load(); }, "earlier deadline interrupts existing wait without audio");
    check(!longer.expired, "later deadline not expired early");
    AAPXSMidi2SessionAccess::forgetRequest(session, 1);
    await([&] { return idle.load(); }, "cancellation removes last scheduled deadline");
    // Let any already signaled dispatch finish before testing that idle stays idle.
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    auto before = dispatches.load();
    std::this_thread::sleep_for(std::chrono::milliseconds(40));
    check(dispatches == before, "no recurring timeout sweep after cancellation");
    worker.stop();
    AAPXSMidi2SessionAccess::setDeadlineChangedHandler(session, {});
}
int main() {
    idleAndSleepRaces(); concurrentQueueWakeups(); sessionDeadlines();
    puts("PASS: event-driven idle/shutdown, concurrent wakeups and exact session deadlines");
}
