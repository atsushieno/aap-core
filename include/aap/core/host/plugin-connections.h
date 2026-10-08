
#ifndef AAP_CORE_PLUGIN_CONNECTIONS_H
#define AAP_CORE_PLUGIN_CONNECTIONS_H

#include <atomic>
#include <mutex>
#include "../plugin-information.h"

namespace aap {

// A list of installed plugins. A snapshot may be shared across threads (e.g. by every
// PluginClient in a host process) and refreshed in place, so accesses are guarded.
// PluginInformation objects are never freed, so pointers returned from a snapshot stay valid
// even after it is refreshed.
class PluginListSnapshot {
    std::vector<const PluginServiceInformation*> services{};
    std::vector<const PluginInformation*> plugins{};
    // The installed plugins generation that this snapshot reflects. 0 means it was never queried.
    uint64_t generation{0};
    mutable std::mutex mutex{};

    static std::atomic<uint64_t> installed_plugins_generation;

public:
    static PluginListSnapshot queryServices();

    // Marks every queried snapshot in the process as stale, so that refreshIfStale() re-queries.
    // It is invoked when plugin packages are installed, updated or removed.
    static void notifyInstalledPluginsChanged();

    PluginListSnapshot() = default;
    PluginListSnapshot(const PluginListSnapshot& other) {
        std::lock_guard<std::mutex> lock{other.mutex};
        services = other.services;
        plugins = other.plugins;
        generation = other.generation;
    }
    PluginListSnapshot& operator=(const PluginListSnapshot& other) {
        if (this == &other)
            return *this;
        std::scoped_lock lock{mutex, other.mutex};
        services = other.services;
        plugins = other.plugins;
        generation = other.generation;
        return *this;
    }

    // Re-queries the installed plugins and replaces the contents.
    void refresh();

    // Re-queries the installed plugins only if they were changed since the last query.
    void refreshIfStale();

    size_t getNumPluginInformation()
    {
        std::lock_guard<std::mutex> lock{mutex};
        return plugins.size();
    }

    const PluginInformation* getPluginInformation(int32_t index)
    {
        std::lock_guard<std::mutex> lock{mutex};
        if (index < 0 || (size_t) index >= plugins.size())
            return nullptr;
        return plugins[(size_t) index];
    }

    const PluginInformation* getPluginInformation(std::string identifier)
    {
        std::lock_guard<std::mutex> lock{mutex};
        for (auto plugin : plugins) {
            if (plugin->getPluginID().compare(identifier) == 0)
                return plugin;
        }
        return nullptr;
    }
};


class PluginClientConnection {
    std::string package_name;
    std::string class_name;
    void * connection_data;

public:
    PluginClientConnection(std::string packageName, std::string className, void * connectionData)
            : package_name(packageName), class_name(className), connection_data(connectionData) {}

    inline std::string & getPackageName() { return package_name; }
    inline std::string & getClassName() { return class_name; }
    inline void * getConnectionData() { return connection_data; }
};

class PluginClientConnectionList {
    std::vector<PluginClientConnection*> serviceConnections{};

public:
    inline void add(std::unique_ptr<PluginClientConnection> entry) {
        serviceConnections.emplace_back(entry.release());
    }

    inline void remove(std::string packageName, std::string className) {
        for (size_t i = 0; i < serviceConnections.size(); i++) {
            auto &c = serviceConnections[i];
            if (c->getPackageName() == packageName && c->getClassName() == className) {
                delete serviceConnections[i];
                serviceConnections.erase(serviceConnections.begin() + i);
                break;
            }
        }
    }

    void* getServiceHandleForConnectedPlugin(std::string packageName, std::string className);

    void* getServiceHandleForConnectedPlugin(std::string pluginId);

    [[maybe_unused]] void* getServiceHandleForConnectedService(std::string pluginId);
};

}

#endif //AAP_CORE_PLUGIN_CONNECTIONS_H
