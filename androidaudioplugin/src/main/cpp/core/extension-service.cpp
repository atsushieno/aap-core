
#include "aap/core/host/audio-plugin-host.h"
#include "aap/core/host/extension-service.h"

namespace aap {

AAPXSClientInstanceWrapper::AAPXSClientInstanceWrapper(RemotePluginInstance* pluginInstance, const char* extensionUri, void* shmData, int32_t shmDataSize)
        : uri(extensionUri), remote_plugin_instance(pluginInstance) {
    client.context = nullptr;
    client.client = nullptr; // FIXME: will we need them?
    client.plugin_instance_id = pluginInstance->getInstanceId();
    client.uri = uri.data();
    client.data = shmData;
    client.data_size = shmDataSize;
}

} // namespace aap
