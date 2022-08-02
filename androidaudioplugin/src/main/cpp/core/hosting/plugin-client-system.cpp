
#include "aap/core/host/plugin-client-system.h"

namespace aap {

std::vector<PluginInformation*> PluginClientSystem::getInstalledPlugins(bool returnCacheIfExists, std::vector<std::string>* searchPaths) {
    std::vector<std::string> aapPaths{};
    for (auto path : getPluginPaths())
        getAAPMetadataPaths(path, aapPaths);
    auto ret = getPluginsFromMetadataPaths(aapPaths);
    return ret;
}

} // namespace aap
