#pragma once
#include <map>
#include <memory>
#include "callback-lifetime.h"

namespace aap { class PluginInstance; class RemotePluginInstance; }
namespace aap::internal {
using RemoteInstanceLifetime = CallbackLifetime<RemotePluginInstance>;
struct RemoteInstanceLifetimes {
    std::mutex mutex;
    std::map<const PluginInstance*, std::shared_ptr<RemoteInstanceLifetime>> items;
};
__attribute__((visibility("hidden"))) inline RemoteInstanceLifetimes& remoteInstanceLifetimes() {
    static auto* registry = new RemoteInstanceLifetimes();
    return *registry;
}
__attribute__((visibility("hidden"))) inline std::shared_ptr<RemoteInstanceLifetime> registerRemoteInstanceLifetime(
        PluginInstance* key, RemotePluginInstance* instance) {
    auto& registry = remoteInstanceLifetimes();
    std::lock_guard<std::mutex> lock{registry.mutex};
    auto& state = registry.items[key];
    if (!state)
        state = std::make_shared<RemoteInstanceLifetime>(instance);
    return state;
}
__attribute__((visibility("hidden"))) inline bool retireRemoteInstanceLifetime(
        PluginInstance* instance, std::function<void()> destroy) {
    auto& registry = remoteInstanceLifetimes();
    std::shared_ptr<RemoteInstanceLifetime> state;
    {
        std::lock_guard<std::mutex> lock{registry.mutex};
        auto found = registry.items.find(instance);
        if (found == registry.items.end())
            return false;
        state = found->second;
        registry.items.erase(found);
    }
    state->retire(std::move(destroy));
    return true;
}
}
