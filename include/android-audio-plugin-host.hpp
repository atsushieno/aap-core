#pragma once

#ifndef _ANDROID_AUDIO_PLUGIN_HOST_HPP_
#define _ANDROID_AUDIO_PLUGIN_HOST_HPP_

#include <stdlib.h>
#include <assert.h>
#include <dlfcn.h>
#include <time.h>
#include <vector>
#include "android/asset_manager.h"
#include "android-audio-plugin.h"


#define _AAP_NULL_TERMINATED_LIST(name) { \
		int32_t n = 0; \
		auto ptr = name; \
		for (;ptr; ptr++) \
			n++; \
		return n; \
	}

namespace aap
{

class PluginInstance;
class AAPEditor;

enum BufferType {
	AAP_BUFFER_TYPE_AUDIO,
	AAP_BUFFER_TYPE_CONTROL
};

enum PortDirection {
	AAP_PORT_DIRECTION_INPUT,
	AAP_PORT_DIRECTION_OUTPUT
};

enum PluginInstantiationState {
	PLUGIN_INSTANTIATION_STATE_UNPREPARED,
	PLUGIN_INSTANTIATION_STATE_INACTIVE,
	PLUGIN_INSTANTIATION_STATE_ACTIVE,
	PLUGIN_INSTANTIATION_STATE_TERMINATED,
};

class PortInformation
{
	const char *name;
	BufferType buffer_type;
	PortDirection direction;
	bool is_control_midi;
	
public:
	const char* getName() { return name; }
	BufferType getBufferType() { return buffer_type; }
	PortDirection getPortDirection() { return direction; }
	bool isControlMidi() { return is_control_midi; }
};

class PluginInformation
{
protected:
	const char *name;
	const char *display_name;
	const char *manufacturer_name;
	const char *version;
	const char *identifier_string;
	const char *shared_library_filename;
	int unique_id;
	long last_info_updated_unixtime;

	/* NULL-terminated list of categories */
	const char *primary_category;
	/* NULL-terminated list of ports */
	PortInformation **ports;
	/* NULL-terminated list of required extensions */
	AndroidAudioPluginExtension **required_extensions;
	/* NULL-terminated list of optional extensions */
	AndroidAudioPluginExtension **optional_extensions;

	// hosting information
	bool is_out_process;

public:
	/* In VST3 world, they are like "Effect", "Synth", "Instrument|Synth", "Fx|Delay" ... can be anything. Here we list typical-looking ones */
	const char * PRIMARY_CATEGORY_EFFECT = "Effect";
	const char * PRIMARY_CATEGORY_SYNTH = "Synth";
	
	PluginInformation(bool isOutProcess)
		: last_info_updated_unixtime((long) time(NULL)),
		  is_out_process(isOutProcess)
	{
	}
	
	const char* getName() const
	{
		return name;
	}
	
	const char* getDescriptiveName() const
	{
		return display_name;
	}
	
	const char* getManufacturerName() const
	{
		return manufacturer_name;
	}
	
	const char* getVersion() const
	{
		return version;
	}
	
	const char* getIdentifier() const
	{
		return identifier_string;
	}
	
	const char* getPrimaryCategory() const
	{
		return primary_category;
	}
	
	int32_t getNumPorts() const
	{
		_AAP_NULL_TERMINATED_LIST(ports)
	}
	
	PortInformation *getPort(int32_t index) const
	{
		assert(index < getNumPorts());
		return ports[index];
	}
	
	int32_t getNumRequiredExtensions() const
	{
		_AAP_NULL_TERMINATED_LIST(required_extensions)
	}
	
	AndroidAudioPluginExtension *getRequiredExtension(int32_t index) const
	{
		assert(index < getNumRequiredExtensions());
		return required_extensions[index];
	}
	
	int32_t getNumOptionalExtensions() const
	{
		_AAP_NULL_TERMINATED_LIST(optional_extensions)
	}
	
	AndroidAudioPluginExtension *getOptionalExtension(int32_t index) const
	{
		assert(index < getNumOptionalExtensions());
		return optional_extensions[index];
	}
	
	long getFileModTime()
	{
		// TODO: implement
		return 0;
	}
	
	long getLastInfoUpdateTime() const
	{
		return last_info_updated_unixtime;
	}
	
	int32_t getUid() const
	{
		return unique_id;
	}
	
	bool isInstrument() const
	{
		// The purpose of this function seems to be based on hacky premise. So do we.
		return strstr(getPrimaryCategory(), "Instrument") != NULL;
	}
	
	bool hasSharedContainer() const
	{
		// TODO (FUTURE): It may be something AAP should support because
		// context switching over outprocess plugins can be quite annoying...
		return false;
	}
	
	bool hasEditor() const
	{
		// TODO: implement
		return false;
	}

	const char* getLocalPluginSharedLibrary() const
	{
		// By metadata or inferred.
		// Since Android expects libraries stored in `lib` directory,
		// it will just return the name of the shared library.
		return shared_library_filename;
	}
	
	bool isOutProcess() const
	{
		return is_out_process;
	}
};

class EditorInstance
{
	PluginInstance *owner;

public:

	EditorInstance(PluginInstance *owner)
		: owner(owner)
	{
		// TODO: implement
	}

	void startEditorUI()
	{
		// TODO: implement
	}
};

class PluginHostBackend
{
};

class PluginHostBackendLV2 : public PluginHostBackend
{
};

class PluginHostBackendVST3 : public PluginHostBackend
{
};

class PluginHost
{
	PluginHostBackendLV2 backend_lv2;
	PluginHostBackendLV2 backend_vst3;

	AAssetManager *asset_manager;
	const char* default_plugin_search_paths[2];
	std::vector<const PluginHostBackend*> backends;
	
	std::vector<const PluginInformation*> plugin_descriptors;
	
	PluginInformation* loadDescriptorFromAssetBundleDirectory(const char *directory);
	PluginInstance* instantiateLocalPlugin(const PluginInformation *descriptor);
	PluginInstance* instantiateRemotePlugin(const PluginInformation *descriptor);

public:

	PluginHost()
		: asset_manager(NULL)
	{
		default_plugin_search_paths[0] = "/";
		default_plugin_search_paths[1] = NULL;
		backends.push_back(&backend_lv2);
		backends.push_back(&backend_vst3);
	}

	~PluginHost()
	{
			for (int i = 0; i < getNumPluginDescriptors(); i++)
				free((void*) getPluginDescriptorAt(i));
	}
	
	void initialize(AAssetManager *assetManager, const char* const *pluginIdentifiers);
	
	bool isPluginAlive (const char *identifier);
	
	bool isPluginUpToDate (const char *identifier, long lastInfoUpdated);

	// Unlike desktop system, it is not practical to either look into file systems
	// on Android. And it is simply impossible to "enumerate" asset directories.
	// Therefore we simply return dummy "/" directory.
	const char** getDefaultPluginSearchPaths()
	{
		return default_plugin_search_paths;
	}
	
	void addHostBackend(const PluginHostBackend *backend)
	{
		assert(backend != NULL);
		backends.push_back(backend);
	}
	
	int32_t getNumHostBackends()
	{
		return backends.size();
	}

	const PluginHostBackend* getHostBackend(int index)
	{
		return backends[index];
	}
	
	int32_t getNumPluginDescriptors()
	{
		return plugin_descriptors.size();
	}

	const PluginInformation* getPluginDescriptorAt(int index)
	{
		return plugin_descriptors[index];
	}
	
	const PluginInformation* getPluginDescriptor(const char *identifier)
	{
		for(int i = 0; i < getNumPluginDescriptors(); i++) {
			auto d = getPluginDescriptorAt(i);
			if (strcmp(d->getIdentifier(), identifier) == 0)
				return d;
		}
		return NULL;
	}
	
	PluginInstance* instantiatePlugin(const char* identifier);
	
	EditorInstance* createEditor(PluginInstance* instance)
	{
		return new EditorInstance(instance);
	}
};

class PluginInstance
{
	friend class PluginHost;
	
	PluginHost *host;
	const PluginInformation *descriptor;
	AndroidAudioPlugin *plugin;
	AAPHandle *instance;
	const AndroidAudioPluginExtension * const *extensions;
	PluginInstantiationState plugin_state;

	PluginInstance(PluginHost* host, const PluginInformation* pluginDescriptor, AndroidAudioPlugin* loadedPlugin)
		: host(host),
		  descriptor(pluginDescriptor),
		  plugin(loadedPlugin),
		  instance(NULL),
		  extensions(NULL),
		  plugin_state(PLUGIN_INSTANTIATION_STATE_UNPREPARED)
	{
	}
	
public:

	const PluginInformation* getPluginDescriptor()
	{
		return descriptor;
	}

	void prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock)
	{
		assert(plugin_state == PLUGIN_INSTANTIATION_STATE_UNPREPARED);
		
		instance = plugin->instantiate(plugin, sampleRate, extensions);
		plugin->prepare(instance);
		plugin_state = PLUGIN_INSTANTIATION_STATE_INACTIVE;
	}
	
	void activate()
	{
		if (plugin_state == PLUGIN_INSTANTIATION_STATE_ACTIVE)
			return;
		assert(plugin_state == PLUGIN_INSTANTIATION_STATE_INACTIVE);
		
		plugin->activate(instance);
		plugin_state = PLUGIN_INSTANTIATION_STATE_ACTIVE;
	}
	
	void deactivate()
	{
		if (plugin_state == PLUGIN_INSTANTIATION_STATE_INACTIVE)
			return;
		assert(plugin_state == PLUGIN_INSTANTIATION_STATE_ACTIVE);
		
		plugin->deactivate(instance);
		plugin_state = PLUGIN_INSTANTIATION_STATE_INACTIVE;
	}
	
	void dispose()
	{
		if (instance != NULL)
			plugin->terminate(instance);
		instance = NULL;
		plugin_state = PLUGIN_INSTANTIATION_STATE_TERMINATED;
	}
	
	void process(AndroidAudioPluginBuffer *buffer, int32_t timeoutInNanoseconds)
	{
		// It is not a TODO here, but if pointers have changed, we have to reconnect
		// LV2 ports.
		plugin->process(instance, buffer, timeoutInNanoseconds);
	}
	
	double getTailLengthSeconds() const
	{
		// TODO: implement
		return 0;
	}
	
	EditorInstance* createEditor()
	{
		return host->createEditor(this);
	}
	
	int getNumPrograms()
	{
		// TODO: FUTURE. LADSPA does not support it either.
		return 0;
	}
	
	int getCurrentProgram()
	{
		// TODO: FUTURE. LADSPA does not support it either.
		return 0;
	}
	
	void setCurrentProgram(int index)
	{
		// TODO: implement. LADSPA does not support it, but resets all parameters.
	}
	
	const char * getProgramName(int index)
	{
		// TODO: FUTURE. LADSPA does not support it either.
		return NULL;
	}
	
	void changeProgramName(int index, const char * newName)
	{
		// implement in case we really care. It seems ignorable.
	}
	
	int32_t getStateSize()
	{
		return plugin->get_state(instance)->data_size;
	}
	
	void const* getState()
	{
		return plugin->get_state(instance)->raw_data;
	}
	
	void setState(const void* data, int32_t offset, int32_t sizeInBytes)
	{
		AndroidAudioPluginState state;
		state.data_size = sizeInBytes;
		state.raw_data = data;
		plugin->set_state(instance, &state);
	}
	
	uint32_t getTailTimeInMilliseconds()
	{
		// TODO: implement
		return 0;
	}
};

} // namespace

#endif // _ANDROID_AUDIO_PLUGIN_HOST_HPP_

