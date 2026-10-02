#include <atomic>
#include <cstdio>
#include <future>
#include <memory>
#include <stdexcept>
#include "callback-lifetime.h"
#include "connection-list-lock.h"
#include "aap/core/host/plugin-connections.h"
#include "aap/core/host/plugin-client-system.h"

// Only the package/class connection lookup is exercised; no platform plugin discovery.
aap::PluginClientSystem* aap::PluginClientSystem::getInstance() { return nullptr; }
std::vector<aap::PluginInformation*> aap::PluginClientSystem::getInstalledPlugins(bool, std::vector<std::string>*) { return {}; }
using namespace aap;
using namespace aap::internal;
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
struct Owner { int value{42}; };
void retirementDuringCallback() {
    for (int i = 0; i < 20; ++i) {
        auto owner = std::make_unique<Owner>();
        auto state = std::make_shared<CallbackLifetime<Owner>>(owner.get());
        std::promise<void> entered, resume, retiring;
        auto resumeFuture = resume.get_future().share();
        std::atomic<int> destroyed{0};
        auto callback = std::async(std::launch::async, [&] {
            state->invoke([&](Owner& live) {
                entered.set_value();
                resumeFuture.wait();
                check(live.value == 42 && destroyed == 0, "callback retains its owner");
            });
        });
        entered.get_future().wait();
        auto retire = std::async(std::launch::async, [&] {
            retiring.set_value();
            state->retire([&] { owner.reset(); ++destroyed; });
        });
        retiring.get_future().wait();
        bool waited = retire.wait_for(std::chrono::milliseconds(10)) == std::future_status::timeout;
        resume.set_value();
        callback.get(); retire.get();
        check(waited && destroyed == 1 && !owner, "retirement waits for the in-flight callback");
        check(!state->invoke([](auto&) { throw std::runtime_error("late callback"); }), "late callback ignored");
    }
}
void reentrantRetirement() {
    auto owner = std::make_unique<Owner>();
    CallbackLifetime<Owner> state{owner.get()};
    int destroyed = 0;
    state.invoke([&](Owner& outer) {
        state.invoke([&](Owner& inner) {
            state.retire([&] { owner.reset(); ++destroyed; });
            check(inner.value == 42 && destroyed == 0, "nested callback stays alive after retirement");
        });
        check(outer.value == 42 && destroyed == 0, "outer callback stays alive after nested retirement");
        check(!state.invoke([](auto&) {}), "retirement prevents new reentrant callbacks");
    });
    check(destroyed == 1 && !owner, "destruction runs after the outermost callback");
    state.retire([&] { ++destroyed; });
    check(destroyed == 1, "retirement runs once");
}
void connectionLookupRace() {
    PluginClientConnectionList list;
    int first = 1, second = 2;
    std::atomic<bool> finished{false};
    auto reader = std::async(std::launch::async, [&] {
        while (!finished.load()) {
            auto connection = list.getServiceHandleForConnectedPlugin("test.package", "test.Class");
            check(!connection || connection == &first || connection == &second, "lookup sees a whole connection entry");
        }
    });
    for (int i = 0; i < 10000; ++i) {
        std::lock_guard<std::recursive_mutex> mutation{connectionListMutex()};
        list.remove("test.package", "test.Class");
        list.add(std::make_unique<PluginClientConnection>("test.package", "test.Class", i % 2 ? &first : &second));
    }
    finished = true; reader.get();
    std::lock_guard<std::recursive_mutex> mutation{connectionListMutex()};
    list.remove("test.package", "test.Class");
    check(!list.getServiceHandleForConnectedPlugin("test.package", "test.Class"), "removed connection lookup is null");
}
int main() {
    retirementDuringCallback(); reentrantRetirement(); connectionLookupRace();
    puts("AAPXS connection lifecycle regression tests passed");
}
