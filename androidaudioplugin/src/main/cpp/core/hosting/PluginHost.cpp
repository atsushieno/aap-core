
#include <sys/stat.h>
#include <aap/core/host/plugin-host.h>
#include <aap/core/host/plugin-instance.h>
#include <aap/core/aapxs/extension-service.h>
#include <aap/core/aapxs/standard-extensions.h>
#include "audio-plugin-host-internals.h"

#define LOG_TAG "AAP.PluginHost"

aap::PluginHost::PluginHost(PluginListSnapshot* contextPluginList,
                            xs::AAPXSDefinitionRegistry* aapxsDefinitionRegistry,
                            int32_t eventMidi2InputBufferSize)
        : plugin_list(contextPluginList),
          event_midi2_input_buffer_size(eventMidi2InputBufferSize) {
    assert(contextPluginList);

    aapxs_definition_registry = aapxsDefinitionRegistry; // FIXME: replace null with standard extensions
}

void aap::PluginHost::destroyInstance(PluginInstance* instance)
{
    instances.erase(std::find(instances.begin(), instances.end(), instance));
    delete instance;
}

aap::PluginInstance* aap::PluginHost::getInstanceByIndex(int32_t index) {
    assert(0 <= index);
    assert(index < instances.size());
    return instances[index];
}

aap::PluginInstance* aap::PluginHost::getInstanceById(int32_t instanceId) {
    for (auto i: instances)
        if (i->getInstanceId() == instanceId)
            return i;
    return nullptr;
}

int32_t localInstanceIdSerial{0};

aap::PluginInstance* aap::PluginHost::instantiateLocalPlugin(const PluginInformation *descriptor, int sampleRate)
{
    dlerror(); // clean up any previous error state
    auto file = descriptor->getLocalPluginSharedLibrary();
    auto metadataFullPath = descriptor->getMetadataFullPath();
    if (!metadataFullPath.empty()) {
        size_t idx = metadataFullPath.find_last_of('/');
        if (idx > 0) {
            auto soFullPath = metadataFullPath.substr(0, idx + 1) + file;
            struct stat st;
            if (stat(soFullPath.c_str(), &st) == 0)
                file = soFullPath;
        }
    }
    auto entrypoint = descriptor->getLocalPluginLibraryEntryPoint();
    auto dl = dlopen(file.length() > 0 ? file.c_str() : "libandroidaudioplugin.so", RTLD_LAZY);
    if (dl == nullptr) {
        aap::a_log_f(AAP_LOG_LEVEL_ERROR, LOG_TAG, "aap::PluginHost: AAP library %s could not be loaded: %s", file.c_str(), dlerror());
        return nullptr;
    }
    auto factoryGetter = (aap_factory_t) dlsym(dl, entrypoint.length() > 0 ? entrypoint.c_str() : "GetAndroidAudioPluginFactory");
    if (factoryGetter == nullptr) {
        aap::a_log_f(AAP_LOG_LEVEL_ERROR, LOG_TAG, "aap::PluginHost: AAP factory entrypoint function %s was not found in %s.", entrypoint.c_str(), file.c_str());
        return nullptr;
    }
    auto pluginFactory = factoryGetter();
    if (pluginFactory == nullptr) {
        aap::a_log_f(AAP_LOG_LEVEL_ERROR, LOG_TAG, "aap::PluginHost: AAP factory entrypoint function %s could not instantiate a plugin.", entrypoint.c_str());
        return nullptr;
    }
    auto instance = new LocalPluginInstance(this,
                                            aapxs_definition_registry,
                                            localInstanceIdSerial++,
                                            descriptor, pluginFactory, sampleRate, event_midi2_input_buffer_size);
    instances.emplace_back(instance);
    return instance;
}
