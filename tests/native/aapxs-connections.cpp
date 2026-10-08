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
#include "plugin-information-registry.h"

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
OwnedPluginInformation makeOwnedPlugin(const char* package, const char* id) {
    OwnedPluginInformation owned{};
    owned.info = std::make_unique<PluginInformation>(true, package, "test.Class", id, "", "1", id, "", "", "", "Effect", "", "", "");
    auto port = owned.ports.emplace_back(std::make_unique<PortInformation>(0, "out", AAP_CONTENT_TYPE_AUDIO, AAP_PORT_DIRECTION_OUTPUT)).get();
    owned.info->addDeclaredPort(port);
    auto parameter = owned.parameters.emplace_back(std::make_unique<ParameterInformation>(0, "gain", 0, 1, 0.5)).get();
    owned.info->addDeclaredParameter(parameter);
    return owned;
}
void pluginInformationRegistry() {
    auto& registry = PluginInformationRegistry::getInstance();
    auto size = registry.size();
    auto first = registry.intern(makeOwnedPlugin("reg.package", "urn:reg"), 100);
    check(first != nullptr && registry.size() == size + 1, "a new plugin is registered");
    for (int i = 0; i < 1000; i++)
        check(registry.intern(makeOwnedPlugin("reg.package", "urn:reg"), 100) == first, "re-queried plugin resolves to the registered one");
    check(registry.size() == size + 1, "re-queries do not grow the registry");
    check(first->getNumDeclaredPorts() == 1 && std::string{first->getDeclaredPort(0)->getName()} == "out", "registered ports stay valid");
    check(first->getNumDeclaredParameters() == 1 && std::string{first->getDeclaredParameter(0)->getName()} == "gain", "registered parameters stay valid");

    auto updated = registry.intern(makeOwnedPlugin("reg.package", "urn:reg"), 200);
    check(updated != first && registry.size() == size + 2, "an updated package gets a new plugin");
    check(first->getPluginID() == "urn:reg", "the plugin of the old installation stays valid");
    check(registry.intern(makeOwnedPlugin("reg.package", "urn:reg"), 200) == updated, "the updated plugin is reused");
    check(registry.intern(makeOwnedPlugin("reg.package", "urn:other"), 200) != updated, "another plugin ID in the package is distinct");
    check(registry.intern(makeOwnedPlugin("reg.other", "urn:reg"), 200) != updated, "another package is distinct");
    auto unknown = registry.intern(makeOwnedPlugin("reg.package", "urn:reg"), 0);
    check(unknown != updated && registry.intern(makeOwnedPlugin("reg.package", "urn:reg"), 0) != unknown, "unknown installation is never shared");
    check(registry.intern(OwnedPluginInformation{}, 100) == nullptr, "empty plugin is rejected");
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
    pluginListRefresh(); pluginListRefreshRace(); pluginInformationRegistry();
    puts("AAPXS connection lifecycle regression tests passed");
}
