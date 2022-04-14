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
#include "plugin-information.h"

namespace aap {

class PluginInstance;

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
	PluginInstance* getInstance(int32_t instanceId) { return instances[(size_t) instanceId]; }
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

	void instantiateRemotePlugin(const PluginInformation *pluginInfo, int sampleRate, std::function<void(PluginInstance*, std::string)> callback);

public:
	PluginClient(PluginClientConnectionList* pluginConnections, PluginListSnapshot* contextPluginList)
		: PluginHost(contextPluginList), connections(pluginConnections)
	{
	}

	inline PluginClientConnectionList* getConnections() { return connections; }

	void createInstanceAsync(std::string identifier, int sampleRate, bool isRemoteExplicit, std::function<void(int32_t, std::string)> callback);
};

// This is persistable AndroidAudioPluginExtension equivalent that can be stored in other persistent objects.
class PluginExtension {
public:
	PluginExtension(AndroidAudioPluginExtension src);

    std::unique_ptr<char> uri{nullptr};
    int32_t dataSize;
    std::unique_ptr<uint8_t> data{nullptr}; // pointer to void does not work...

    AndroidAudioPluginExtension asTransient() const;
};

class PluginBuffer;

class PluginInstance
{
	friend class PluginHost;
	friend class PluginClient;
	friend class PluginListSnapshot;

	int sample_rate{44100};
	const PluginInformation *pluginInfo;
	AndroidAudioPluginFactory *plugin_factory;
	AndroidAudioPlugin *plugin;
	std::vector<std::unique_ptr<PluginExtension>> extensions{};
	PluginInstantiationState instantiation_state;
	AndroidAudioPluginState plugin_state{0, nullptr};
	std::unique_ptr<PluginBuffer> plugin_buffer{nullptr};

	PluginInstance(const PluginInformation* pluginInformation, AndroidAudioPluginFactory* loadedPluginFactory, int sampleRate);

	int32_t allocateAudioPluginBuffer(size_t numPorts, size_t numFrames);

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

	int getNumPrograms()
	{
		// TODO: FUTURE (v0.6). LADSPA does not support it either.
		return 0;
	}
	
	int getCurrentProgram()
	{
		// TODO: FUTURE (v0.6). LADSPA does not support it either.
		return 0;
	}
	
	void setCurrentProgram(int /*index*/)
	{
		// TODO: FUTURE (v0.6). LADSPA does not support it, but resets all parameters.
	}

	std::string getProgramName(int /*index*/)
	{
		// TODO: FUTURE (v0.6). LADSPA does not support it either.
		return nullptr;
	}
	
	void changeProgramName(int /*index*/, std::string /*newName*/)
	{
		// TODO: FUTURE (v0.6). LADSPA does not support it either.
	}

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


} // namespace

#endif // _ANDROID_AUDIO_PLUGIN_HOST_HPP_

