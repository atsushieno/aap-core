#include <atomic>
#include <cstdio>
#include <future>
#include <functional>
#include <memory>
#include <mutex>
#include <stdexcept>
#include "callback-lifetime.h"
#include "connection-list-lock.h"
#include "aap/core/host/plugin-connections.h"
#include "aap/core/host/plugin-client-system.h"

// Platform plugin discovery is replaced by a fixed list that the tests modify.
namespace {
struct StubClientSystem : aap::PluginClientSystem {
    int32_t createSharedMemory(size_t) override { return -1; }
    void ensurePluginServiceConnected(aap::PluginClientConnectionList*, std::string, std::function<void(std::string&)>) override {}
    std::vector<std::string> getPluginPaths() override { return {}; }
    void getAAPMetadataPaths(std::string, std::vector<std::string>&) override {}
    std::vector<aap::PluginInformation*> getPluginsFromMetadataPaths(std::vector<std::string>&) override { return {}; }
} stub_client_system;
std::mutex installed_mutex;
std::vector<aap::PluginInformation*> installed_plugins;
std::atomic<int> installed_queries{0};
}
aap::PluginClientSystem* aap::PluginClientSystem::getInstance() { return &stub_client_system; }
std::vector<aap::PluginInformation*> aap::PluginClientSystem::getInstalledPlugins(bool, std::vector<std::string>*) {
    ++installed_queries;
    std::lock_guard<std::mutex> lock{installed_mutex};
    return installed_plugins;
}
using namespace aap;
using namespace aap::internal;
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
PluginInformation* makePlugin(const char* id) {
    return new PluginInformation(true, "test.package", "test.Class", id, "", "1", id, "", "", "", "Effect", "", "", "");
}
void installPlugin(PluginInformation* plugin) {
    std::lock_guard<std::mutex> lock{installed_mutex};
    installed_plugins.emplace_back(plugin);
}
void pluginListRefresh() {
    auto first = makePlugin("urn:first");
    installPlugin(first);
    PluginListSnapshot list;
    check(list.getPluginInformation("urn:first") == nullptr, "an unqueried snapshot is empty");
    int queries = installed_queries;
    list.refreshIfStale();
    check(installed_queries == queries + 1, "an unqueried snapshot is stale");
    check(list.getPluginInformation("urn:first") == first, "refresh finds the installed plugin");
    list.refreshIfStale();
    check(installed_queries == queries + 1, "an up-to-date snapshot is not re-queried");

    // installed without notification: not visible until an explicit refresh.
    auto second = makePlugin("urn:second");
    installPlugin(second);
    list.refreshIfStale();
    check(list.getPluginInformation("urn:second") == nullptr, "an up-to-date snapshot keeps its contents");
    list.refresh();
    check(list.getPluginInformation("urn:second") == second, "explicit refresh finds a new plugin");

    auto third = makePlugin("urn:third");
    installPlugin(third);
    PluginListSnapshot::notifyInstalledPluginsChanged();
    list.refreshIfStale();
    check(list.getPluginInformation("urn:third") == third, "notified change refreshes a stale snapshot");
    check(first->getPluginID() == "urn:first", "pointers from earlier queries stay valid");
    check(list.getPluginInformation(3) == nullptr && list.getPluginInformation(-1) == nullptr, "out of range index is null");

    PluginListSnapshot copy{list};
    check(copy.getNumPluginInformation() == 3 && copy.getPluginInformation("urn:third") == third, "copy keeps contents");
}
void pluginListRefreshRace() {
    PluginListSnapshot list;
    list.refresh();
    auto plugin = list.getPluginInformation("urn:first");
    std::atomic<bool> finished{false};
    auto reader = std::async(std::launch::async, [&] {
        while (!finished.load())
            check(list.getPluginInformation("urn:first") == plugin, "lookup during refresh sees a whole list");
    });
    for (int i = 0; i < 2000; ++i) {
        PluginListSnapshot::notifyInstalledPluginsChanged();
        list.refreshIfStale();
    }
    finished = true; reader.get();
}
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
    pluginListRefresh(); pluginListRefreshRace();
    puts("AAPXS connection lifecycle regression tests passed");
}
