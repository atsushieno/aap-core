#ifndef AAP_CORE_PLUGIN_CLIENT_SYSTEM_H
#define AAP_CORE_PLUGIN_CLIENT_SYSTEM_H

#include <sys/stat.h>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>
#include "../plugin-information.h"
#include "plugin-connections.h"

namespace aap {

class PluginClientSystem
{
public:
    static PluginClientSystem* getInstance();

    virtual int32_t createSharedMemory(size_t size) = 0;

    // Ensures that the primary AudioPluginService of the package `serviceName` is bound (the one
    // declared with the stock `org.androidaudioplugin.AudioPluginService` class, or the first one),
    // then invokes `callback`. A plugin package may have more than one AudioPluginService (in
    // separate processes), and the plugins of the others are not reachable through it.
    // Prefer the overload that takes the service class name.
    virtual void ensurePluginServiceConnected(aap::PluginClientConnectionList* connections, std::string serviceName, std::function<void(std::string&)> callback) = 0;

    virtual std::vector<std::string> getPluginPaths() = 0;
    virtual void getAAPMetadataPaths(std::string path, std::vector<std::string>& results) = 0;
    virtual std::vector<PluginInformation*> getPluginsFromMetadataPaths(std::vector<std::string>& aapMetadataPaths) = 0;

    std::vector<PluginInformation*> getInstalledPlugins(bool returnCacheIfExists = true, std::vector<std::string>* searchPaths = nullptr);

    // Ensures that the AudioPluginService `packageName`/`className` is bound, then invokes `callback`.
    // For a plugin, they are PluginInformation::getPluginPackageName() and getPluginLocalName().
    // An empty `className` binds the primary AudioPluginService of the package (see above).
    // (It is declared after the other virtual functions to keep the existing vtable layout.)
    virtual void ensurePluginServiceConnected(aap::PluginClientConnectionList* connections, std::string packageName, std::string className, std::function<void(std::string&)> callback) = 0;
};

} // namespace aap

#endif //AAP_CORE_PLUGIN_CLIENT_SYSTEM_H
