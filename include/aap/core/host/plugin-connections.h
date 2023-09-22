
#ifndef AAP_CORE_PLUGIN_CONNECTIONS_H
#define AAP_CORE_PLUGIN_CONNECTIONS_H

#include "../plugin-information.h"

namespace aap {

class PluginListSnapshot {
    std::vector<const PluginServiceInformation*> services{};
    std::vector<const PluginInformation*> plugins{};

public:
    static PluginListSnapshot queryServices();

    size_t getNumPluginInformation()
    {
        return plugins.size();
    }

    const PluginInformation* getPluginInformation(int32_t index)
    {
        return plugins[(size_t) index];
    }

    const PluginInformation* getPluginInformation(std::string identifier)
    {
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
