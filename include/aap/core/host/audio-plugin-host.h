#pragma once

#ifndef _ANDROID_AUDIO_PLUGIN_HOST_HPP_
#define _ANDROID_AUDIO_PLUGIN_HOST_HPP_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <dlfcn.h>
#include <time.h>
#include <unistd.h>
#include <memory>
#include <vector>
#include <map>
#include <string>
#include "aap/android-audio-plugin.h"
#include "aap/port-properties.h"
#include "aap/unstable/logging.h"
#include "aap/unstable/presets.h"
#include "aap/unstable/state.h"
#include "plugin-connections.h"
#include "../aapxs/extension-service.h"
#include "../aapxs/standard-extensions.h"

namespace aap {

//-------------------------------------------------------

class PluginInstance;
class LocalPluginInstance;
class RemotePluginInstance;

/* Common foundation for both Plugin service and Plugin client. */
class PluginHost
{
	std::unique_ptr<AAPXSRegistry> aapxs_registry;

protected:
	PluginListSnapshot* plugin_list{nullptr};
	std::vector<PluginInstance*> instances{};
	PluginInstance* instantiateLocalPlugin(const PluginInformation *pluginInfo, int sampleRate);

public:
	PluginHost(PluginListSnapshot* contextPluginList);

	virtual ~PluginHost() {}

	AAPXSFeature* getExtensionFeature(const char* uri);

	void destroyInstance(PluginInstance* instance);

	size_t getInstanceCount() { return instances.size(); }

	inline LocalPluginInstance* getInstance(int32_t instanceId) {
		return (LocalPluginInstance*) instances[(size_t) instanceId];
	}
};


class PluginService : public PluginHost {
public:
	PluginService(PluginListSnapshot* contextPluginList)
			: PluginHost(contextPluginList)
	{
	}

	int createInstance(std::string identifier, int sampleRate);
};

class PluginClient : public PluginHost {
	PluginClientConnectionList* connections;

	void instantiateRemotePlugin(bool canDynamicallyConnect, const PluginInformation *pluginInfo, int sampleRate, std::function<void(PluginInstance*, std::string)> callback);

	void createInstanceImpl(bool canDynamicallyConnect, std::string identifier, int sampleRate, bool isRemoteExplicit, std::function<void(int32_t, std::string)> callback);
public:
	PluginClient(PluginClientConnectionList* pluginConnections, PluginListSnapshot* contextPluginList)
		: PluginHost(contextPluginList), connections(pluginConnections)
	{
	}

	inline PluginClientConnectionList* getConnections() { return connections; }

    template<typename T>
    struct Result {
        T value;
        std::string error;
    };

	// Synchronous version that does not expect service connection on the fly (fails immediately).
	// It is probably better suited for Kotlin client to avoid complicated JNI interop.
	Result<int32_t> createInstance(std::string identifier, int sampleRate, bool isRemoteExplicit);

	// Asynchronous version that allows service connection on the fly.
	void createInstanceAsync(std::string identifier, int sampleRate, bool isRemoteExplicit, std::function<void(int32_t, std::string)> callback);
};

//-------------------------------------------------------

class PluginBuffer;
class AAPXSSharedMemoryStore;

class PluginInstance
{
	friend class PluginHost;
	friend class PluginClient;
	friend class PluginListSnapshot;
    friend class LocalPluginInstanceStandardExtensions;
	friend class RemotePluginInstanceStandardExtensions;

	int sample_rate{44100};
	int instance_id;
	const PluginInformation *pluginInfo;
	AndroidAudioPluginFactory *plugin_factory;
	AAPXSSharedMemoryStore *aapxs_shared_memory_store;
	PluginInstantiationState instantiation_state;
	std::unique_ptr<PluginBuffer> plugin_buffer{nullptr};

	int32_t allocateAudioPluginBuffer(size_t numPorts, size_t numFrames);

protected:
	AndroidAudioPlugin *plugin;

	PluginInstance(int32_t instanceId, const PluginInformation* pluginInformation, AndroidAudioPluginFactory* loadedPluginFactory, int sampleRate);

	virtual AndroidAudioPluginHost* getHostFacadeForCompleteInstantiation() = 0;

public:

    virtual ~PluginInstance();

	inline int32_t getInstanceId() { return instance_id; }

	inline AAPXSSharedMemoryStore* getAAPXSSharedMemoryStore() { return aapxs_shared_memory_store; }

    // It may or may not be shared memory buffer. Available only after prepare().
	AndroidAudioPluginBuffer* getAudioPluginBuffer(size_t numPorts, size_t numFrames);

	const PluginInformation* getPluginInformation()
	{
		return pluginInfo;
	}

	void completeInstantiation();

	void prepare(int maximumExpectedSamplesPerBlock, AndroidAudioPluginBuffer *preparedBuffer)
	{
		assert(instantiation_state == PLUGIN_INSTANTIATION_STATE_UNPREPARED || instantiation_state == PLUGIN_INSTANTIATION_STATE_INACTIVE);

		plugin->prepare(plugin, preparedBuffer);
		instantiation_state = PLUGIN_INSTANTIATION_STATE_INACTIVE;
	}
	
	void activate()
	{
		if (instantiation_state == PLUGIN_INSTANTIATION_STATE_ACTIVE)
			return;
		assert(instantiation_state == PLUGIN_INSTANTIATION_STATE_INACTIVE);
		
		plugin->activate(plugin);
		instantiation_state = PLUGIN_INSTANTIATION_STATE_ACTIVE;
	}
	
	void deactivate()
	{
		if (instantiation_state == PLUGIN_INSTANTIATION_STATE_INACTIVE)
			return;
		assert(instantiation_state == PLUGIN_INSTANTIATION_STATE_ACTIVE);
		
		plugin->deactivate(plugin);
		instantiation_state = PLUGIN_INSTANTIATION_STATE_INACTIVE;
	}
	
	void dispose();

	void process(AndroidAudioPluginBuffer *buffer, int32_t timeoutInNanoseconds)
	{
		plugin->process(plugin, buffer, timeoutInNanoseconds);
	}

	virtual StandardExtensions& getStandardExtensions() = 0;

	uint32_t getTailTimeInMilliseconds()
	{
		// TODO: FUTURE (v0.7) - most likely just a matter of plugin property
		return 0;
	}
};

class LocalPluginInstanceStandardExtensionsImpl : public LocalPluginInstanceStandardExtensions {
	LocalPluginInstance *owner;

public:
	LocalPluginInstanceStandardExtensionsImpl(LocalPluginInstance* owner)
			: owner(owner) {
	}
	AndroidAudioPlugin* getPlugin() override;
};

/**
 * A plugin instance that could use dlopen() and dlsym(). It can be either client side or host side.
 */
class LocalPluginInstance : public PluginInstance {
	PluginHost *service;
	AndroidAudioPluginHost plugin_host_facade{};
	AAPXSInstanceMap<AAPXSServiceInstance> aapxsServiceInstances;
	LocalPluginInstanceStandardExtensionsImpl standards;

	// FIXME: should we commonize these members with ClientPluginInstance?
	//  They only differ at getExtension() or getExtensionService() so far.
	template<typename T> T withPresetsExtension(T defaultValue, std::function<T(aap_presets_extension_t*, AndroidAudioPluginExtensionTarget)> func) {
		auto presetsExt = (aap_presets_extension_t*) plugin->get_extension(plugin, AAP_PRESETS_EXTENSION_URI);
		if (presetsExt == nullptr)
			return defaultValue;
		return func(presetsExt, AndroidAudioPluginExtensionTarget{plugin, nullptr});
	}

	inline static void* internalGetExtensionData(AndroidAudioPluginHost *host, const char* uri) {
		return nullptr;
	}

protected:
	AndroidAudioPluginHost* getHostFacadeForCompleteInstantiation() override;

public:
	LocalPluginInstance(PluginHost *service, int32_t instanceId, const PluginInformation* pluginInformation, AndroidAudioPluginFactory* loadedPluginFactory, int sampleRate);

	inline AndroidAudioPlugin* getPlugin() { return plugin; }

	// unlike client host side, this function is invoked for each `addExtension()` Binder call,
	// which is way simpler.
	AAPXSServiceInstance* setupAAPXSInstance(AAPXSFeature *feature, int32_t dataSize = -1) {
		const char* uri = aapxsServiceInstances.addOrGetUri(feature->uri);
		assert (aapxsServiceInstances.get(uri) == nullptr);
		if (dataSize < 0)
			dataSize = feature->shared_memory_size;
		aapxsServiceInstances.add(uri, std::make_unique<AAPXSServiceInstance>(AAPXSServiceInstance{this, uri, getInstanceId(), nullptr, dataSize}));
		return aapxsServiceInstances.get(uri);
	}

	AAPXSServiceInstance* getInstanceFor(const char* uri) {
		auto ret = aapxsServiceInstances.get(uri);
		assert(ret);
		return ret;
	}

	StandardExtensions& getStandardExtensions() override { return standards; }

	// It is invoked by AudioPluginInterfaceImpl, and supposed to dispatch request to extension service
	void controlExtension(const std::string &uri, int32_t opcode)
	{
		auto aapxsInstance = getInstanceFor(uri.c_str());
		auto feature = service->getExtensionFeature(uri.c_str());
		feature->on_invoked(feature, getPlugin(), aapxsInstance, opcode);
	}
};

class RemotePluginInstanceStandardExtensionsImpl : public RemotePluginInstanceStandardExtensions {
	RemotePluginInstance *owner;

public:
	RemotePluginInstanceStandardExtensionsImpl(RemotePluginInstance* owner)
			: owner(owner) {
	}

	AndroidAudioPlugin* getPlugin() override;
	AAPXSClientInstanceManager* getAAPXSManager() override;
};

class RemotePluginInstance : public PluginInstance {
	PluginClient *client;
	AndroidAudioPluginHost plugin_host_facade{};
    RemotePluginInstanceStandardExtensionsImpl standards;
    std::unique_ptr<AAPXSClientInstanceManager> aapxs_manager;

    friend class RemoteAAPXSManager;
protected:
	AndroidAudioPluginHost* getHostFacadeForCompleteInstantiation() override;

public:
	// The `instantiate()` member of the plugin factory is supposed to invoke `setupAAPXSInstances()`.
	// (binder-client-as-plugin does so, and desktop implementation should do so too.)
    RemotePluginInstance(PluginClient *client, int32_t instanceId, const PluginInformation* pluginInformation, AndroidAudioPluginFactory* loadedPluginFactory, int sampleRate);

    /** it is an unwanted exposure, but we need this internal-only member as public. You are not supposed to use it. */
    std::function<void(const char*, int32_t, int32_t)> send_extension_message_impl;

	inline AndroidAudioPlugin* getPlugin() { return plugin; }

	inline PluginClient* getClient() { return client; }

    AAPXSClientInstanceManager* getAAPXSManager() { return aapxs_manager.get(); }

    void sendExtensionMessage(const char *uri, int32_t opcode);

	StandardExtensions& getStandardExtensions() override { return standards; }
};

class RemoteAAPXSManager : public AAPXSClientInstanceManager {
    RemotePluginInstance *owner;

protected:

    static void staticSendExtensionMessage(AAPXSClientInstance* clientInstance, int32_t opcode);

public:
    RemoteAAPXSManager(RemotePluginInstance* owner)
            : owner(owner) {
    }

    const PluginInformation* getPluginInformation() override { return owner->getPluginInformation(); }
    AndroidAudioPlugin* getPlugin() override { return owner->plugin; }
    AAPXSFeature* getExtensionFeature(const char* uri) override {
        return owner->getClient()->getExtensionFeature(uri);
    }
    AAPXSClientInstance* setupAAPXSInstance(AAPXSFeature *feature, int32_t dataSize) override;
};

} // namespace

#endif // _ANDROID_AUDIO_PLUGIN_HOST_HPP_

