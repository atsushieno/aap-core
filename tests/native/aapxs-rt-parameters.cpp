#include <atomic>
#include <cstdio>
#include <stdexcept>
#include <thread>
#include "aap/core/realtime.h"
#include "parameter-value-cache.h"

using namespace aap::internal;
void check(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
void retainedSnapshots() {
    ParameterValueCache cache;
    cache.publish({{17, 0, 100, 30}, {42, -1, 1, 0}});
    auto* before = cache.snapshot();
    {
        aap::RealtimeScope rt;
        check(cache.getByIndex(0) == 30, "initial values prepared off processing");
        check(cache.setById(17, 88.123456789), "stable ID update accepted");
    }
    cache.publish({{42, -1, 1, 0}, {17, 0, 100, 30}, {29, 0, 1, 0.5}});
    {
        aap::RealtimeScope rt;
        check(cache.getByIndex(1) == 88.123456789, "reordering retains full double precision");
        check(before->setById(17, 91), "retained snapshot remains usable");
        check(cache.getByIndex(1) == 91, "old snapshot updates same cell after publication");
        check(cache.getByIndex(2) == 0.5, "new parameter initialized before publication");
    }
    cache.publish({});
    check(cache.getByIndex(0) == 0 && !cache.setById(17, 12), "valid empty layout");
    cache.publish({{17, 0, 100, 30}});
    check(cache.getByIndex(0) == 91, "reappearing stable ID retains value");
}
void concurrentPublication() {
    ParameterValueCache cache;
    cache.publish({{17, 0, 100, 0}, {42, -1, 1, 0}});
    std::atomic<bool> done{false};
    std::thread publisher([&] {
        for (int i = 0; i < 5000; ++i)
            if (i % 2) cache.publish({{17, 0, 100, 0}, {42, -1, 1, 0}});
            else cache.publish({{42, -1, 1, 0}, {17, 0, 100, 0}, {29, 0, 1, 0.5}});
        done = true;
    });
    uint32_t updates = 0;
    while (!done) {
        aap::RealtimeScope rt;
        auto* snapshot = cache.snapshot();
        check(snapshot->setById(17, ++updates), "all revisions contain stable ID");
        check(snapshot->entries.size() >= 2 && snapshot->id_to_index.size() == snapshot->entries.size(), "complete immutable publication");
    }
    publisher.join();
    auto* final = cache.snapshot();
    check(cache.getByIndex(final->id_to_index.at(17)) == updates, "publication never loses latest update");
}
int main() {
    retainedSnapshots(); concurrentPublication();
    puts("PASS: realtime parameter values and concurrent immutable layout publication");
}
