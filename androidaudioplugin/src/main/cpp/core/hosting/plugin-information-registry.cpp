#include "plugin-information-registry.h"

namespace aap::internal {

PluginInformationRegistry& PluginInformationRegistry::getInstance() {
    static PluginInformationRegistry instance{};
    return instance;
}

PluginInformation* PluginInformationRegistry::intern(OwnedPluginInformation&& plugin, int64_t packageLastUpdateTime) {
    if (!plugin.info)
        return nullptr;
    std::lock_guard<std::mutex> lock{mutex};
    if (packageLastUpdateTime != 0) {
        auto info = plugin.info.get();
        for (auto& entry : entries) {
            auto existing = entry->plugin.info.get();
            if (entry->package_last_update_time == packageLastUpdateTime &&
                existing->getPluginPackageName() == info->getPluginPackageName() &&
                existing->getPluginLocalName() == info->getPluginLocalName() &&
                existing->getPluginID() == info->getPluginID())
                return existing; // `plugin` is discarded.
        }
    }
    auto entry = std::make_unique<Entry>(Entry{packageLastUpdateTime, std::move(plugin)});
    auto ret = entry->plugin.info.get();
    entries.emplace_back(std::move(entry));
    return ret;
}

size_t PluginInformationRegistry::size() {
    std::lock_guard<std::mutex> lock{mutex};
    return entries.size();
}

} // namespace aap::internal
