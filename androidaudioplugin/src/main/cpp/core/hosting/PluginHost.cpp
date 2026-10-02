
#include <sys/stat.h>
#include <aap/core/host/plugin-host.h>
#include <aap/core/host/plugin-instance.h>
#include <aap/core/aapxs/extension-service.h>
#include <aap/core/aapxs/standard-extensions.h>
#include "audio-plugin-host-internals.h"
#include "host-aapxs-request-queue.h"
#include "plugin-parameter-state.h"
#include "remote-instance-lifetime.h"

#define LOG_TAG "AAP.PluginHost"

aap::PluginHost::PluginHost(PluginListSnapshot* contextPluginList,
                            xs::AAPXSDefinitionRegistry* aapxsDefinitionRegistry,
                            int32_t eventMidi2InputBufferSize)
        : plugin_list(contextPluginList),
          event_midi2_input_buffer_size(eventMidi2InputBufferSize) {
    if (!contextPluginList) {
        AAP_ASSERT_FALSE;
        return;
    }

    aapxs_definition_registry = aapxsDefinitionRegistry ? aapxsDefinitionRegistry : xs::AAPXSDefinitionRegistry::getStandardExtensions();
}

namespace {

struct AAPXSInstanceContext {
    AAPXSDefinition* definition;
    void* context;
};

template <typename Dispatcher, typename GetInitiator, typename GetRecipient>
void collectAAPXSInstanceContexts(aap::xs::AAPXSDefinitionRegistry* registry, Dispatcher& dispatcher,
                                  GetInitiator getInitiator, GetRecipient getRecipient,
                                  std::vector<AAPXSInstanceContext>& result) {
    for (auto& definition : *registry) {
        if (!definition.uri || !definition.release_instance_context)
            continue;
        if (auto initiator = getInitiator(dispatcher, definition.uri); initiator && initiator->aapxs_context)
            result.push_back({&definition, initiator->aapxs_context});
        if (auto recipient = getRecipient(dispatcher, definition.uri); recipient && recipient->aapxs_context)
            result.push_back({&definition, recipient->aapxs_context});
    }
}

// Empty dispatchers return null. Collect initialized contexts even if creation failed
// partway through, so a rejected factory result does not leak extension-owned contexts.
std::vector<AAPXSInstanceContext> collectAAPXSInstanceContexts(aap::PluginInstance* instance) {
    std::vector<AAPXSInstanceContext> result;
    if (auto local = dynamic_cast<aap::LocalPluginInstance*>(instance))
        collectAAPXSInstanceContexts(local->getAAPXSRegistry()->items(), local->getAAPXSDispatcher(),
                                     [](auto& d, const char* uri) { return d.getHostAAPXSByUri(uri); },
                                     [](auto& d, const char* uri) { return d.getPluginAAPXSByUri(uri); },
                                     result);
    else if (auto remote = dynamic_cast<aap::RemotePluginInstance*>(instance))
        collectAAPXSInstanceContexts(remote->getAAPXSRegistry()->items(), remote->getAAPXSDispatcher(),
                                     [](auto& d, const char* uri) { return d.getPluginAAPXSByUri(uri); },
                                     [](auto& d, const char* uri) { return d.getHostAAPXSByUri(uri); },
                                     result);
    return result;
}

}

void aap::PluginHost::destroyInstance(PluginInstance* instance)
{
    auto found = std::find(instances.begin(), instances.end(), instance);
    if (found == instances.end())
        return;
    instances.erase(found);
    auto destroy = [instance] {
        // The plugin may hold pointers into these contexts until it is released (at `delete`).
        auto aapxsContexts = collectAAPXSInstanceContexts(instance);
        internal::closeParameterLayoutRefresh(*instance);
        delete instance;
        for (auto& c : aapxsContexts)
            c.definition->release_instance_context(c.definition, c.context);
        internal::HostAAPXSRequestQueue::getInstance().forgetOwner(instance);
        internal::forgetParameterLayoutRefresh(*instance);
    };
    if (!internal::retireRemoteInstanceLifetime(instance, destroy))
        destroy();
}

aap::PluginInstance* aap::PluginHost::getInstanceByIndex(int32_t index) {
    if (index < 0 || index >= instances.size()) {
        AAP_ASSERT_FALSE;
        return nullptr;
    }
    return instances[index];
}

aap::PluginInstance* aap::PluginHost::getInstanceById(int32_t instanceId) {
    for (auto i: instances)
        if (i->getInstanceId() == instanceId)
            return i;
    return nullptr;
}

int32_t localInstanceIdSerial{0};

aap::PluginInstance* aap::PluginHost::instantiateLocalPlugin(const PluginInformation *descriptor)
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
                                            descriptor, pluginFactory, event_midi2_input_buffer_size);
    instances.emplace_back(instance);
    return instance;
}
