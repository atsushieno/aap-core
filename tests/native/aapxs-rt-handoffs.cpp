#include <atomic>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <thread>
#include <vector>
#include "aap/core/realtime.h"
#include "realtime-byte-queue.h"
#include "processing-quiescence.h"
#include "instance-extension-worker.h"

using namespace aap::internal;
void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
void queueBoundaries() {
    RealtimeByteQueue<4> queue(8);
    uint32_t prefix = 123;
    {
        aap::RealtimeScope rt;
        for (uint32_t value = 0; value < 4; ++value)
            check(queue.tryPush(&value, 4, &prefix, 4), "all slots available");
        check(!queue.tryPush(&prefix, 4), "full queue rejects without fallback");
        check(!queue.tryPush(&prefix, 9), "oversized record rejected");
        check(!queue.tryConsume([](auto*, auto) { return false; }), "head can be retained");
        for (uint32_t expected = 0; expected < 4; ++expected)
            check(queue.tryConsume([&](void* data, size_t size) {
                check(size == 8 && reinterpret_cast<uintptr_t>(data) % 8 == 0, "aligned copied record");
                auto words = static_cast<uint32_t*>(data);
                check(words[0] == 123 && words[1] == expected, "FIFO and prefix preserved");
                return true;
            }), "queued slot consumed");
        check(!queue.tryConsume([](auto*, auto) { return true; }), "empty queue returns immediately");
        check(queue.tryPush(&prefix, 4), "consumed capacity reusable");
    }
    check(queue.rejectedCount() == 2, "backpressure observable");
}
void concurrentProducers() {
    struct Record { uint32_t producer, serial, checksum; };
    RealtimeByteQueue<64> queue(sizeof(Record));
    constexpr uint32_t producers = 4, count = 20000;
    std::atomic<uint32_t> finished{0}, accepted{0};
    std::vector<std::thread> threads;
    for (uint32_t producer = 0; producer < producers; ++producer)
        threads.emplace_back([&, producer] {
            for (uint32_t serial = 0; serial < count; ++serial) {
                Record record{producer, serial, producer ^ serial ^ 0xf3ef12};
                aap::RealtimeScope rt;
                check(aap::RealtimeScope::isActive(), "concurrent processing scope registered");
                { aap::RealtimeScope nested; check(aap::RealtimeScope::isActive(), "nested scope registered"); }
                check(aap::RealtimeScope::isActive(), "outer scope retained after nested exit");
                if (queue.tryPush(&record, sizeof(record))) ++accepted;
            }
            ++finished;
        });
    uint32_t consumed = 0, last[producers]{};
    bool seen[producers]{};
    auto consume = [&](void* data, size_t size) {
        auto record = *static_cast<Record*>(data);
        check(size == sizeof(Record) && record.producer < producers, "valid published record");
        check(record.checksum == (record.producer ^ record.serial ^ 0xf3ef12), "no torn copies");
        check(!seen[record.producer] || record.serial > last[record.producer], "per-producer order preserved");
        seen[record.producer] = true;
        last[record.producer] = record.serial;
        ++consumed;
        return true;
    };
    while (finished != producers) {
        check(!aap::RealtimeScope::isActive(), "other threads do not annotate consumer");
        queue.tryConsume(consume);
    }
    for (auto& thread : threads) thread.join();
    while (queue.tryConsume(consume)) {}
    check(consumed == accepted, "all accepted records delivered once");
    check(accepted + queue.rejectedCount() == producers * count, "every submission accounted for");
}
void controlExclusion() {
    ProcessingQuiescence state;
    std::atomic<bool> inside{false}, release{false}, controlEntered{false};
    std::thread process([&] {
        aap::RealtimeScope rt;
        ProcessingQuiescence::Process activity(state);
        check(bool(activity), "initial processing enters");
        inside = true;
        while (!release) std::this_thread::yield(); // controlled test plugin
    });
    while (!inside) std::this_thread::yield();
    std::thread control([&] {
        ProcessingQuiescence::Control pause(state);
        controlEntered = true;
        check(release.load(), "control waits for earlier block");
        ProcessingQuiescence::Control nested(state);
        for (int i = 0; i < 10000; ++i) {
            aap::RealtimeScope rt;
            ProcessingQuiescence::Process skipped(state);
            check(!skipped, "processing returns during control without waiting");
        }
    });
    // Wait until suspension has been published, without timing assumptions.
    while (true) {
        ProcessingQuiescence::Process probe(state);
        if (!probe) break;
    }
    check(!controlEntered, "running block excludes control");
    release = true;
    process.join();
    control.join();
    ProcessingQuiescence::Process resumed(state);
    check(bool(resumed), "processing resumes after nested control");
}
void workerHandoff() {
    RealtimeByteQueue<4> queue(4);
    InstanceExtensionWorker worker;
    std::atomic<bool> callbackEntered{false}, release{false}, drained{false};
    worker.start([&] {
        queue.tryConsume([&](void*, size_t) {
            check(!aap::RealtimeScope::isActive(), "callback runs outside processing");
            callbackEntered = true;
            while (!release) std::this_thread::yield();
            return true;
        });
        if (release) drained = true;
    });
    uint32_t value = 7;
    { aap::RealtimeScope rt; check(queue.tryPush(&value, 4), "processing copies handoff"); }
    while (!callbackEntered) std::this_thread::yield();
    { aap::RealtimeScope rt; check(queue.tryPush(&value, 4), "blocked callback does not block producer"); }
    release = true;
    while (!drained) std::this_thread::yield();
    worker.stop();
}
int main() {
    queueBoundaries(); concurrentProducers(); controlExclusion(); workerHandoff();
    puts("PASS: bounded realtime queues, control exclusion and worker handoff");
}
