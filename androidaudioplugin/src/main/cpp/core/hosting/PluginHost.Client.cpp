
#include <aap/core/host/plugin-client-system.h>
#include <aap/core/host/plugin-host.h>
#include <aap/core/host/plugin-instance.h>
#include "audio-plugin-host-internals.h"

void aap::PluginClient::connectToPluginService(const std::string& identifier, std::function<void(std::string&)> callback) {
    const PluginInformation *descriptor = plugin_list->getPluginInformation(identifier);
    assert (descriptor != nullptr);
    connectToPluginService(descriptor->getPluginPackageName(), descriptor->getPluginLocalName(), callback);
}
void aap::PluginClient::connectToPluginService(const std::string& packageName, const std::string& className, std::function<void(std::string&)> callback) {
    auto service = connections->getServiceHandleForConnectedPlugin(packageName, className);
    if (service != nullptr) {
        std::string error{};
        callback(error);
    } else {
        PluginClientSystem::getInstance()->ensurePluginServiceConnected(connections, packageName, callback);
    }
}

void aap::PluginClient::createInstanceAsync(std::string identifier, int sampleRate, bool isRemoteExplicit, std::function<void(int32_t, std::string&)>& userCallback)
{
    std::function<void(std::string&)> cb = [identifier,sampleRate,isRemoteExplicit,userCallback,this](std::string& error) {
        if (error.empty()) {
            auto result = createInstance(identifier, sampleRate, isRemoteExplicit);
            userCallback(result.value, result.error);
        }
        else
            userCallback(-1, error);
    };
    connectToPluginService(identifier, cb);
}

aap::PluginClient::Result<int32_t> aap::PluginClient::createInstance(std::string identifier, int sampleRate, bool isRemoteExplicit)
{
    Result<int32_t> result;
    const PluginInformation *descriptor = plugin_list->getPluginInformation(identifier);
    assert (descriptor != nullptr);

    // For local plugins, they can be directly loaded using dlopen/dlsym.
    // For remote plugins, the connection has to be established through binder.
    auto internalCallback = [=](PluginInstance* instance, std::string error) {
        if (instance != nullptr) {
            return Result<int32_t>{instance->getInstanceId(), ""};
        }
        else
            return Result<int32_t>{-1, error};
    };
    if (isRemoteExplicit || descriptor->isOutProcess())
        return instantiateRemotePlugin(descriptor, sampleRate);
    else {
        try {
            auto instance = instantiateLocalPlugin(descriptor, sampleRate);
            return internalCallback(instance, "");
        } catch(std::exception& ex) {
            return internalCallback(nullptr, ex.what());
        }
    }
}

aap::PluginClient::Result<int32_t> aap::PluginClient::instantiateRemotePlugin(const PluginInformation *descriptor, int sampleRate)
{
    // We first ensure to bind the remote plugin service, and then create a plugin instance.
    //  Since binding the plugin service must be asynchronous while instancing does not have to be,
    //  we call ensureServiceConnected() first and pass the rest as its callback.
    auto internalCallback = [this,descriptor,sampleRate](std::string error) {
        if (error.empty()) {
#if ANDROID
            auto pluginFactory = GetAndroidAudioPluginFactoryClientBridge(this);
#else
            auto pluginFactory = GetDesktopAudioPluginFactoryClientBridge(this);
#endif
            assert (pluginFactory != nullptr);
            auto instance = new RemotePluginInstance(this,
#if USE_AAPXS_V2
                                        aapxs_definition_registry,
#else
                                                     aapxs_registry,
#endif
                                                     descriptor, pluginFactory,
                                                     sampleRate, event_midi2_input_buffer_size);
            instances.emplace_back(instance);
            instance->completeInstantiation();
            instance->scanParametersAndBuildList();
            return Result<int32_t>{instance->getInstanceId(), ""};
        }
        else
            return Result<int32_t>{-1, error};
    };
    auto service = connections->getServiceHandleForConnectedPlugin(descriptor->getPluginPackageName(), descriptor->getPluginLocalName());
    if (service != nullptr)
        return internalCallback("");
    else
        return internalCallback(std::string{"Plugin service is not started yet: "} + descriptor->getPluginID());
}

