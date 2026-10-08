#ifndef AAP_CORE_PLUGIN_INFORMATION_REGISTRY_H
#define AAP_CORE_PLUGIN_INFORMATION_REGISTRY_H

#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>
#include "aap/core/plugin-information.h"

namespace aap::internal {

// A PluginInformation together with the declared ports and parameters it refers to
// (PluginInformation itself does not own them).
struct OwnedPluginInformation {
    std::unique_ptr<PluginInformation> info{};
    std::vector<std::unique_ptr<PortInformation>> ports{};
    std::vector<std::unique_ptr<ParameterInformation>> parameters{};
};

// Owns the PluginInformation objects queried from the platform, for the process lifetime.
// Hosts and plugin instances hold raw PluginInformation pointers beyond any PluginListSnapshot,
// so they are never freed. Instead, a re-queried plugin from the same package installation
// is resolved to the registered object, so that repeated queries do not grow memory.
class PluginInformationRegistry {
    struct Entry {
        int64_t package_last_update_time;
        OwnedPluginInformation plugin;
    };
    std::mutex mutex{};
    std::vector<std::unique_ptr<Entry>> entries{};

public:
    static PluginInformationRegistry& getInstance();

    // Returns the registered plugin of the same package, class, plugin ID and package
    // installation (packageLastUpdateTime), or registers `plugin` if there is none.
    // An unknown packageLastUpdateTime (0) never matches an existing entry.
    PluginInformation* intern(OwnedPluginInformation&& plugin, int64_t packageLastUpdateTime);

    size_t size();
};

} // namespace aap::internal

#endif //AAP_CORE_PLUGIN_INFORMATION_REGISTRY_H
