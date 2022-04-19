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
#include "aap/unstable/presets.h"
#include "plugin-information.h"
#include "extension-service.h"

namespace aap {

//-------------------------------------------------------

// This is persistable AndroidAudioPluginExtension equivalent that can be stored in other persistent objects.
class PluginExtension {
public:
	PluginExtension(AndroidAudioPluginExtension src);

	std::unique_ptr<char> uri{nullptr};
	int32_t dataSize;
	std::unique_ptr<uint8_t> data{nullptr}; // pointer to void does not work...

	AndroidAudioPluginExtension asTransient() const;
};

//-------------------------------------------------------

class PluginInstance;
class LocalPluginInstance;

/* Common foundation for both Plugin service and Plugin client. */
class PluginHost
{
protected:
	PluginListSnapshot* plugin_list{nullptr};
	std::vector<PluginInstance*> instances{};
	PluginInstance* instantiateLocalPlugin(const PluginInformation *pluginInfo, int sampleRate);

public:
	PluginHost(PluginListSnapshot* contextPluginList) : plugin_list(contextPluginList)
	{
	}

	virtual ~PluginHost() {}

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

// FIXME: is this for plugin client only, plugin service only, or both?
class PluginExtensionServiceRegistry;

class PluginClient : public PluginHost {
	PluginClientConnectionList* connections;
	std::unique_ptr<PluginExtensionServiceRegistry> extension_registry;

	void instantiateRemotePlugin(const PluginInformation *pluginInfo, int sampleRate, std::function<void(PluginInstance*, std::string)> callback);

public:
	PluginClient(PluginClientConnectionList* pluginConnections, PluginListSnapshot* contextPluginList)
		: PluginHost(contextPluginList), connections(pluginConnections)
	{
		extension_registry = std::make_unique<PluginExtensionServiceRegistry>();
	}

	inline PluginClientConnectionList* getConnections() { return connections; }

	void createInstanceAsync(std::string identifier, int sampleRate, bool isRemoteExplicit, std::function<void(int32_t, std::string)> callback);

	AAPXSFeatureWrapper getExtensionFeature(const char* uri);
};

//-------------------------------------------------------

class PluginBuffer;

class PluginInstance
{
	friend class PluginHost;
	friend class PluginClient;
	friend class PluginListSnapshot;

	int sample_rate{44100};
	const PluginInformation *pluginInfo;
	AndroidAudioPluginFactory *plugin_factory;
	std::vector<std::unique_ptr<PluginExtension>> extensions{};
	PluginInstantiationState instantiation_state;
	AndroidAudioPluginState plugin_state{0, nullptr};
	std::unique_ptr<PluginBuffer> plugin_buffer{nullptr};

	int32_t allocateAudioPluginBuffer(size_t numPorts, size_t numFrames);

protected:
	AndroidAudioPlugin *plugin;

	PluginInstance(const PluginInformation* pluginInformation, AndroidAudioPluginFactory* loadedPluginFactory, int sampleRate);

	virtual AndroidAudioPluginHost* getHostFacadeForCompleteInstantiation() = 0;

public:

    virtual ~PluginInstance();

    // It may or may not be shared memory buffer. Available only after prepare().
	AndroidAudioPluginBuffer* getAudioPluginBuffer(size_t numPorts, size_t numFrames);

	const PluginInformation* getPluginInformation()
	{
		return pluginInfo;
	}

	void addExtension(AndroidAudioPluginExtension ext)
	{
		extensions.emplace_back(std::make_unique<PluginExtension>(ext));
	}

	void completeInstantiation();

	const void* getExtension(const char* uri)
	{
		for (auto& ext : extensions)
			if (strcmp(ext->uri.get(), uri) == 0)
				return ext->data.get();
		return nullptr;
	}

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
	
	void dispose()
	{
		instantiation_state = PLUGIN_INSTANTIATION_STATE_TERMINATED;
		if (plugin != nullptr)
			plugin_factory->release(plugin_factory, plugin);
		plugin = nullptr;
	}
	
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
	std::unique_ptr<PluginExtensionServiceRegistry> registry;
	AndroidAudioPluginHost plugin_host_facade{};
	std::map<const char*, AndroidAudioPluginExtension> extension_instances;

	// FIXME: should we commonize these members with ClientPluginInstance?
	//  They only differ at getExtension() or getExtensionService() so far.
	template<typename T> T withPresetsExtension(T defaultValue, std::function<T(aap_presets_extension_t*, aap_presets_context_t*)> func) {
		auto presetsExt = (aap_presets_extension_t*) getExtension(AAP_PRESETS_EXTENSION_URI);
		if (presetsExt == nullptr)
			return defaultValue;
		aap_presets_context_t context;
		context.context = this;
		context.plugin = plugin;
		return func(presetsExt, &context);
	}

	inline static void* internalGetExtension(AndroidAudioPluginHost *host, const char* uri) {
		auto thisObj = (LocalPluginInstance*) host->context;
		return const_cast<void*>(thisObj->getExtension(uri));
	}

protected:
	AndroidAudioPluginHost* getHostFacadeForCompleteInstantiation() override;

public:
	LocalPluginInstance(const PluginInformation* pluginInformation, AndroidAudioPluginFactory* loadedPluginFactory, int sampleRate);

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

	void controlExtension(const std::string &uri, int32_t opcode)
	{
		// FIXME: implement
	}
};

class RemotePluginInstance : public PluginInstance {
	PluginClient *client;
	std::unique_ptr<AAPXSClientInstanceWrapper> aapxsClientInstanceWrapper;

	template<typename T> T withPresetsExtension(T defaultValue, std::function<T(aap_presets_extension_t*, aap_presets_context_t*)> func) {
		auto presetsExt = (aap_presets_extension_t*) getExtensionProxy(AAP_PRESETS_EXTENSION_URI);
		if (presetsExt == nullptr)
			return defaultValue;
		aap_presets_context_t context;
		context.context = this;
		context.plugin = plugin;
		return func(presetsExt, &context);
	}

    void sendExtensionMessage(AAPXSClientInstanceWrapper *aapxsWrapper, int32_t opcode) {
        // FIXME: implement
		assert(false);
    }

	static void internalExtensionMessage (void* context, int32_t opcode) {
		auto thisObj = (RemotePluginInstance *) context;
		auto aapxsInstance = (AAPXSClientInstanceWrapper *) thisObj;
		auto client = aapxsInstance->getPluginInstance();
		client->sendExtensionMessage(aapxsInstance, opcode);
	}

protected:
	AndroidAudioPluginHost* getHostFacadeForCompleteInstantiation() override;

public:
    RemotePluginInstance(PluginClient *client, const PluginInformation* pluginInformation, AndroidAudioPluginFactory* loadedPluginFactory, int sampleRate)
		: PluginInstance(pluginInformation, loadedPluginFactory, sampleRate), client(client) {
	}

	AAPXSClientInstanceWrapper* getExtensionProxyWrapper(const char* uri) {
		assert(plugin != nullptr); // should not be invoked before completeInstantiation().

		if (aapxsClientInstanceWrapper == nullptr) {
			// FIXME: pass correct `data` and `size`.
			aapxsClientInstanceWrapper = std::make_unique<AAPXSClientInstanceWrapper>(this, uri, nullptr, 0);
		}
		return aapxsClientInstanceWrapper.get();
	}

	// For host developers, it is the only entry point to get extension.
	// Therefore the return value is the (strongly typed) extension proxy value.
	// When we AAP developers implement the internals, we need another wrapper around this function.
	//
	// FIXME: this should really be renamed to `getExtension()` but that conflicts an existing function.
	void* getExtensionProxy(const char* uri) {
        auto aapxsClientInstance = getExtensionProxyWrapper(uri)->asPublicApi();
		return client->getExtensionFeature(uri).data().as_proxy(aapxsClientInstance);
	}

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

