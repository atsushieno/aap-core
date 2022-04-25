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
#include "plugin-information.h"
#include "extension-service.h"

namespace aap {

//-------------------------------------------------------

class PluginInstance;
class LocalPluginInstance;

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

	AAPXSFeatureWrapper getExtensionFeature(const char* uri);

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
	std::vector<std::unique_ptr<AndroidAudioPluginExtension>> common_host_extensions{};

	void instantiateRemotePlugin(const PluginInformation *pluginInfo, int sampleRate, std::function<void(PluginInstance*, std::string)> callback);

public:
	PluginClient(PluginClientConnectionList* pluginConnections, PluginListSnapshot* contextPluginList)
		: PluginHost(contextPluginList), connections(pluginConnections)
	{
	}

	inline PluginClientConnectionList* getConnections() { return connections; }

	void createInstanceAsync(std::string identifier, int sampleRate, bool isRemoteExplicit, std::function<void(int32_t, std::string)> callback);

	void addCommonHostExtension(const char *uri, int32_t sharedDataSizeToAllocate) {
		AndroidAudioPluginExtension ext{uri, sharedDataSizeToAllocate, nullptr};
		common_host_extensions.emplace_back(std::make_unique<AndroidAudioPluginExtension>(ext));
	}

	std::vector<AndroidAudioPluginExtension*> getCommonHostExtensions() {
		auto ret = std::vector<AndroidAudioPluginExtension*>{};
		for (auto &x : common_host_extensions)
			ret.emplace_back(x.get());
		return ret;
	}
};

//-------------------------------------------------------

class PluginBuffer;
class AAPXSSharedMemoryStore;

class PluginInstance
{
	friend class PluginHost;
	friend class PluginClient;
	friend class PluginListSnapshot;

	int sample_rate{44100};
	int instance_id;
	const PluginInformation *pluginInfo;
	AndroidAudioPluginFactory *plugin_factory;
	AAPXSSharedMemoryStore *aapxs_shared_memory_store;
	PluginInstantiationState instantiation_state;
	AndroidAudioPluginState plugin_state{0, nullptr};
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
		// It is not a TODO here, but if pointers have changed, we have to reconnect
		// LV2 ports.
		plugin->process(plugin, buffer, timeoutInNanoseconds);
	}

	// FIXME: maybe move these standard presets features to some class.

	virtual int32_t getPresetCount() = 0;
	virtual int32_t getCurrentPresetIndex() = 0;
	virtual void setCurrentPresetIndex(int index) = 0;
	virtual std::string getCurrentPresetName(int index) = 0;

	size_t getStateSize()
	{
		AndroidAudioPluginState result{0, nullptr};
		plugin->get_state(plugin, &result);
		return result.data_size;
	}

	const AndroidAudioPluginState& getState()
	{
		AndroidAudioPluginState result{0, nullptr};
		plugin->get_state(plugin, &result);
		if (plugin_state.data_size < result.data_size) {
			if (plugin_state.raw_data != nullptr)
				free((void*) plugin_state.raw_data);
			plugin_state.raw_data = calloc(result.data_size, 1);
		}
		plugin_state.data_size = result.data_size;
		plugin->get_state(plugin, &plugin_state);

		return plugin_state;
	}
	
	void setState(const void* data, size_t sizeInBytes)
	{
		AndroidAudioPluginState state;
		state.data_size = sizeInBytes;
		state.raw_data = data;
		plugin->set_state(plugin, &state);
	}
	
	uint32_t getTailTimeInMilliseconds()
	{
		// TODO: FUTURE (v0.6) - most likely just a matter of plugin property
		return 0;
	}
};

/**
 * A plugin instance that could use dlopen() and dlsym(). It can be either client side or host side.
 */
class LocalPluginInstance : public PluginInstance {
	PluginHost *service;
	AndroidAudioPluginHost plugin_host_facade{};
	AAPXSInstanceMap<AAPXSServiceInstanceWrapper> aapxsServiceInstanceWrappers;
	std::vector<std::unique_ptr<AndroidAudioPluginExtension>> host_extensions{};

	// FIXME: should we commonize these members with ClientPluginInstance?
	//  They only differ at getExtension() or getExtensionService() so far.
	template<typename T> T withPresetsExtension(T defaultValue, std::function<T(aap_presets_extension_t*, aap_presets_context_t*)> func) {
		auto presetsExt = (aap_presets_extension_t*) plugin->get_extension(plugin, AAP_PRESETS_EXTENSION_URI);
		if (presetsExt == nullptr)
			return defaultValue;
		aap_presets_context_t context;
		context.context = this;
		context.plugin = plugin;
		return func(presetsExt, &context);
	}

	inline static void* internalGetHostExtension(AndroidAudioPluginHost *host, const char* uri) {
		auto thisObj = (LocalPluginInstance*) host->context;
		return const_cast<void*>(thisObj->getHostExtension(uri));
	}

protected:
	AndroidAudioPluginHost* getHostFacadeForCompleteInstantiation() override;

public:
	LocalPluginInstance(PluginHost *service, int32_t instanceId, const PluginInformation* pluginInformation, AndroidAudioPluginFactory* loadedPluginFactory, int sampleRate);

	inline AndroidAudioPlugin* getPlugin() { return plugin; }

	void prepareExtension(AndroidAudioPluginExtension ext)
	{
		host_extensions.emplace_back(std::make_unique<AndroidAudioPluginExtension>(ext));
	}

	const void* getHostExtension(const char* uri)
	{
		for (auto& ext : host_extensions)
			if (strcmp(ext->uri, uri) == 0)
				return ext->data;
		return nullptr;
	}

	AAPXSServiceInstanceWrapper* getAAPXSWrapper(const char* uri) {
		assert(plugin != nullptr); // should not be invoked before completeInstantiation().

		if (aapxsServiceInstanceWrappers.get(uri) == nullptr)
			// We pass null data and 0 size here, but will be initialized later at AudioPluginInterfaceImpl.
			aapxsServiceInstanceWrappers.add(uri, std::make_unique<AAPXSServiceInstanceWrapper>(this, uri, nullptr, 0));
		return aapxsServiceInstanceWrappers.get(uri);
	}

	// FIXME: we should probably move them to dedicated class for standard extensions.

	int32_t getPresetCount() override
	{
		return withPresetsExtension<int32_t>(0, [&](aap_presets_extension_t* ext, aap_presets_context_t *ctx) {
			return ext->get_preset_count(ctx);
		});
	}

	int32_t getCurrentPresetIndex() override
	{
		return withPresetsExtension<int32_t>(0, [&](aap_presets_extension_t* ext, aap_presets_context_t *ctx) {
			return ext->get_preset_index(ctx);
		});
	}

	void setCurrentPresetIndex(int index) override
	{
		withPresetsExtension<int32_t>(0, [&](aap_presets_extension_t* ext, aap_presets_context_t *ctx) {
			ext->set_preset_index(ctx, index);
			return 0;
		});
	}

	std::string getCurrentPresetName(int index) override
	{
		return withPresetsExtension<std::string>("", [&](aap_presets_extension_t* ext, aap_presets_context_t *ctx) {
			aap_preset_t result;
			ext->get_preset(ctx, index, true, &result);
			return std::string{result.name};
		});
	}

	// It is invoked by AudioPluginInterfaceImpl, and supposed to dispatch request to extension service
	void controlExtension(const std::string &uri, int32_t opcode)
	{
		auto extensionWrapper = getAAPXSWrapper(uri.c_str());
		auto feature = service->getExtensionFeature(uri.c_str());
		feature.data().on_invoked(service, extensionWrapper->asPublicApi(), opcode);
	}
};

class RemotePluginInstance : public PluginInstance {
	PluginClient *client;
	AAPXSInstanceMap<AAPXSClientInstanceWrapper> aapxsClientInstanceWrappers{};
	AndroidAudioPluginHost plugin_host_facade{};

	inline static void* internalGetExtensionProxy(AndroidAudioPluginHost *host, const char* uri) {
		auto thisObj = (RemotePluginInstance*) host->context;
		return const_cast<void*>(thisObj->getExtensionProxy(uri));
	}

	template<typename T> T withPresetsExtension(T defaultValue, std::function<T(aap_presets_extension_t*, aap_presets_context_t*)> func) {
		auto presetsExt = (aap_presets_extension_t*) getExtensionProxy(AAP_PRESETS_EXTENSION_URI);
		if (presetsExt == nullptr)
			return defaultValue;
		aap_presets_context_t context;
		context.context = this;
		context.plugin = plugin;
		return func(presetsExt, &context);
	}

    void sendExtensionMessage(const char *uri, int32_t opcode) {
		auto aapxsInstance = (AAPXSClientInstanceWrapper *) getAAPXSWrapper(uri);
		// Here we have to get a native plugin instance and send extension message.
		// It is kind af annoying because we used to implement Binder-specific part only within the
		// plugin API (binder-client-as-plugin.cpp)...
		// So far, instead of rewriting a lot of code to do so, we let AAPClientContext
		// assign its implementation details that handle Binder messaging as a std::function.
		send_extension_message_impl(aapxsInstance, getInstanceId(), opcode);
    }

protected:
	AndroidAudioPluginHost* getHostFacadeForCompleteInstantiation() override;

public:
	// The `instantiate()` member of the plugin factory is supposed to invoke `setupAAPXSInstances()`.
	// (binder-client-as-plugin does so, and desktop implementation should do so too.)
    RemotePluginInstance(PluginClient *client, int32_t instanceId, const PluginInformation* pluginInformation, AndroidAudioPluginFactory* loadedPluginFactory, int sampleRate)
		: PluginInstance(instanceId, pluginInformation, loadedPluginFactory, sampleRate), client(client) {
	}

	/** it is an unwanted exposure, but we need this internal-only member as public. You are not supposed to use it. */
	std::function<void(AAPXSClientInstanceWrapper*, int32_t, int32_t)> send_extension_message_impl;

	AAPXSClientInstanceWrapper* setupAAPXSWrapper(const char* uri, int32_t dataSize) {
		assert (aapxsClientInstanceWrappers.get(uri) == nullptr);
		aapxsClientInstanceWrappers.add(uri, std::make_unique<AAPXSClientInstanceWrapper>(this, uri, nullptr, dataSize));
		return aapxsClientInstanceWrappers.get(uri);
	}

	AAPXSClientInstanceWrapper* getAAPXSWrapper(const char* uri) {
		auto ret = aapxsClientInstanceWrappers.get(uri);
		assert(ret);
		return ret;
	}

	// For host developers, it is the only entry point to get extension.
	// Therefore the return value is the (strongly typed) extension proxy value.
	// When we AAP developers implement the internals, we need another wrapper around this function.
	void* getExtensionProxy(const char* uri) {
		auto aapxsWrapper = getAAPXSWrapper(uri);
		if (!aapxsWrapper) {
			aap::a_log_f(AAP_LOG_LEVEL_INFO, "AAP", "AAPXS Proxy for extension '%s' is not found", uri);
			return nullptr;
		}
		auto aapxsClientInstance = aapxsWrapper->asPublicApi();
		assert(aapxsClientInstance);
		auto wrapper = client->getExtensionFeature(uri);
		assert(strlen(wrapper.getUri()) > 0);
		auto feature = wrapper.data();
		assert(feature.as_proxy != nullptr);
		auto proxy = feature.as_proxy(aapxsClientInstance);
		return proxy;
	}

	// It is invoked by AAP framework (actually binder-client-as-plugin) to set up AAPXS for each
	// supported extension, while leaving RemotePluginClient to determine what extensions to provide.
	// It is called at completeInstantiation() step
	void setupAAPXSInstances(std::function<void(AAPXSClientInstance*)> func) {
		// FIXME: we should also query registered extension services so that it does not crash
		//  when a plugin service is queried at use time.
		auto pluginInfo = getPluginInformation();
		for (auto hostExt: client->getCommonHostExtensions()) {
			func(setupAAPXSWrapper(hostExt->uri, hostExt->transmit_size)->asPublicApi());
		}
		for (int i = 0, n = pluginInfo->getNumExtensions(); i < n; i++) {
			auto info = pluginInfo->getExtension(i);
			// We pass null data and 0 size here, but will be initialized later at binder-client-as-plugin.cpp.
			func(setupAAPXSWrapper(info.uri.c_str(), 0)->asPublicApi());
		}
	}

	// FIXME: we should probably move them to dedicated class for standard extensions.

	int32_t getPresetCount() override
	{
		return withPresetsExtension<int32_t>(0, [&](aap_presets_extension_t* ext, aap_presets_context_t *ctx) {
			return ext->get_preset_count(ctx);
		});
	}

	int32_t getCurrentPresetIndex() override
	{
		return withPresetsExtension<int32_t>(0, [&](aap_presets_extension_t* ext, aap_presets_context_t *ctx) {
			return ext->get_preset_index(ctx);
		});
	}

	void setCurrentPresetIndex(int index) override
	{
		withPresetsExtension<int32_t>(0, [&](aap_presets_extension_t* ext, aap_presets_context_t *ctx) {
			ext->set_preset_index(ctx, index);
			return 0;
		});
	}

	std::string getCurrentPresetName(int index) override
	{
		return withPresetsExtension<std::string>("", [&](aap_presets_extension_t* ext, aap_presets_context_t *ctx) {
			aap_preset_t result;
			ext->get_preset(ctx, index, true, &result);
			return std::string{result.name};
		});
	}
};

} // namespace

#endif // _ANDROID_AUDIO_PLUGIN_HOST_HPP_

