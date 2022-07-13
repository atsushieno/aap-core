
#include "aap/core/host/plugin-connections.h"
#include "plugin-client-system.h"

namespace aap {

PluginListSnapshot PluginListSnapshot::queryServices() {
    PluginListSnapshot ret{};
    for (auto p : PluginClientSystem::getInstance()->getInstalledPlugins())
        ret.plugins.emplace_back(p);
    return ret;
}

void* PluginClientConnectionList::getServiceHandleForConnectedPlugin(std::string packageName, std::string className)
{
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
