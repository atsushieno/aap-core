
#include <aap/core/host/plugin-host.h>
#include <aap/core/host/plugin-instance.h>

#define LOG_TAG "AAP.PluginHost.Service"

int32_t aap::PluginService::createInstance(std::string identifier, int sampleRate)  {
    auto info = plugin_list->getPluginInformation(identifier);
    if (!info) {
        aap::a_log_f(AAP_LOG_LEVEL_ERROR, LOG_TAG, "aap::PluginService: Plugin information was not found for: %s ", identifier.c_str());
        return -1;
    }
    auto instance = instantiateLocalPlugin(info, sampleRate);
    return instance->getInstanceId();
}
