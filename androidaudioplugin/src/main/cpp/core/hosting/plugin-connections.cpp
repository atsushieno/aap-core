
#include "aap/core/host/plugin-connections.h"
#include "aap/core/host/plugin-client-system.h"
#include "connection-list-lock.h"

namespace aap {

std::atomic<uint64_t> PluginListSnapshot::installed_plugins_generation{1};

PluginListSnapshot PluginListSnapshot::queryServices() {
    PluginListSnapshot ret{};
    // Read the generation before querying, so that a change notified during the query leaves it stale.
    ret.generation = installed_plugins_generation.load();
    for (auto p : PluginClientSystem::getInstance()->getInstalledPlugins())
        ret.plugins.emplace_back(p);
    return ret;
}

void PluginListSnapshot::notifyInstalledPluginsChanged() {
    installed_plugins_generation.fetch_add(1);
}

void PluginListSnapshot::refresh() {
    // Query without holding the lock, as it involves PackageManager queries.
    auto updated = queryServices();
    std::lock_guard<std::mutex> lock{mutex};
    services = std::move(updated.services);
    plugins = std::move(updated.plugins);
    generation = updated.generation;
}

void PluginListSnapshot::refreshIfStale() {
    bool stale;
    {
        std::lock_guard<std::mutex> lock{mutex};
        stale = generation != installed_plugins_generation.load();
    }
    if (stale)
        refresh();
}

void* PluginClientConnectionList::getServiceHandleForConnectedPlugin(std::string packageName, std::string className)
{
    const std::lock_guard<std::recursive_mutex> lock{internal::connectionListMutex()};
    for (int i = 0; i < serviceConnections.size(); i++) {
        auto s = serviceConnections[i];
        if (s->getPackageName() == packageName && s->getClassName() == className)
            return serviceConnections[i]->getConnectionData();
    }
    return nullptr;
}

void* PluginClientConnectionList::getServiceHandleForConnectedPlugin(std::string pluginId)
{
    auto pl = PluginClientSystem::getInstance()->getInstalledPlugins();
    for (auto &plugin : pl)
        if (plugin->getPluginID() == pluginId)
            return getServiceHandleForConnectedPlugin(plugin->getPluginPackageName(),
                                                      plugin->getPluginLocalName());
    return nullptr;
}

} // namespace aap
