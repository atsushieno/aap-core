
#ifndef AAP_CORE_PLUGIN_SERVICE_LIST_H
#define AAP_CORE_PLUGIN_SERVICE_LIST_H

#include "aap/core/host/audio-plugin-host.h"

namespace aap {

class PluginServiceList {
    std::vector<PluginService*> bound_plugin_service_list{};

public:
    static PluginServiceList* getInstance();

    void addBoundServiceInProcess(PluginService* service) {
        bound_plugin_service_list.emplace_back(service);
    }

    void removeBoundServiceInProcess(PluginService* service) {
        auto i = std::find(bound_plugin_service_list.begin(), bound_plugin_service_list.end(), service);
        bound_plugin_service_list.erase(i);
    }

    PluginService* findBoundServiceInProcess(const char* pluginId) {
        for (auto s : bound_plugin_service_list)
            if (true) // FIXME: find exact Service instance
                return s;
        return nullptr;
    }
};

}

#endif //AAP_CORE_PLUGIN_SERVICE_LIST_H
