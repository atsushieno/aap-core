#ifndef AAP_CORE_PLUGIN_CLIENT_SYSTEM_H
#define AAP_CORE_PLUGIN_CLIENT_SYSTEM_H

#include <sys/stat.h>
#include <cstdint>
#include <vector>
#include "../plugin-information.h"
#include "plugin-connections.h"

namespace aap {

class PluginClientSystem
{
public:
    static PluginClientSystem* getInstance();

    virtual int32_t createSharedMemory(size_t size) = 0;

    virtual void ensurePluginServiceConnected(aap::PluginClientConnectionList* connections, std::string serviceName, std::function<void(std::string&)> callback) = 0;

    virtual std::vector<std::string> getPluginPaths() = 0;
    virtual void getAAPMetadataPaths(std::string path, std::vector<std::string>& results) = 0;
    virtual std::vector<PluginInformation*> getPluginsFromMetadataPaths(std::vector<std::string>& aapMetadataPaths) = 0;

    std::vector<PluginInformation*> getInstalledPlugins(bool returnCacheIfExists = true, std::vector<std::string>* searchPaths = nullptr);
};

} // namespace aap

#endif //AAP_CORE_PLUGIN_CLIENT_SYSTEM_H
