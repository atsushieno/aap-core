#pragma once

#include <stdlib.h>
#include <assert.h>
#include <dlfcn.h>
#include <time.h>
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

class AAPInstance;
class AAPEditor;

enum AAPBufferType {
	AAP_BUFFER_TYPE_AUDIO,
	AAP_BUFFER_TYPE_CONTROL
};

enum AAPPortDirection {
	AAP_PORT_DIRECTION_INPUT,
	AAP_PORT_DIRECTION_OUTPUT
};

enum PluginInstantiationState {
	PLUGIN_INSTANTIATION_STATE_UNPREPARED,
	PLUGIN_INSTANTIATION_STATE_INACTIVE,
	PLUGIN_INSTANTIATION_STATE_ACTIVE,
	PLUGIN_INSTANTIATION_STATE_TERMINATED,
};

class AAPPortDescriptor
{
	const char *name;
	AAPBufferType buffer_type;
	AAPPortDirection direction;
	bool is_control_midi;
	
public:
	const char* getName() { return name; }
	AAPBufferType getBufferType() { return buffer_type; }
	AAPPortDirection getPortDirection() { return direction; }
	bool isControlMidi() { return is_control_midi; }
};

class AAPDescriptor
{
protected:
	const char *name;
	const char *display_name;
	const char *manufacturer_name;
	const char *version;
	const char *identifier_string;
	int unique_id;
	long last_info_updated_unixtime;

	/* NULL-terminated list of categories */
	const char *primary_category;
	/* NULL-terminated list of ports */
	AAPPortDescriptor **ports;
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
	
	AAPDescriptor(bool isOutProcess)
		: last_info_updated_unixtime((long) time(NULL)),
		  is_out_process(isOutProcess)
	{
	}
	
	const char* getName()
	{
		return name;
	}
	
	const char* getDescriptiveName()
	{
		return display_name;
	}
	
	const char* getManufacturerName()
	{
		return manufacturer_name;
	}
	
	const char* getVersion()
	{
		return version;
	}
	
	const char* getIdentifier()
	{
		return identifier_string;
	}
	
	const char* getPrimaryCategory()
	{
		return primary_category;
	}
	
	int32_t getNumPorts()
	{
		_AAP_NULL_TERMINATED_LIST(ports)
	}
	
	AAPPortDescriptor *getPort(int32_t index)
	{
		assert(index < getNumPorts());
		return ports[index];
	}
	
	int32_t getNumRequiredExtensions()
	{
		_AAP_NULL_TERMINATED_LIST(required_extensions)
	}
	
	AndroidAudioPluginExtension *getRequiredExtension(int32_t index)
	{
		assert(index < getNumRequiredExtensions());
		return required_extensions[index];
	}
	
	int32_t getNumOptionalExtensions()
	{
		_AAP_NULL_TERMINATED_LIST(optional_extensions)
	}
	
	AndroidAudioPluginExtension *getOptionalExtension(int32_t index)
	{
		assert(index < getNumOptionalExtensions());
		return optional_extensions[index];
	}
	
	long getFileModTime()
	{
		// TODO: implement
		return 0;
	}
	
	long getLastInfoUpdateTime()
	{
		return last_info_updated_unixtime;
	}
	
	int32_t getUid()
	{
		return unique_id;
	}
	
	bool isInstrument()
	{
		// TODO: implement. Probably by metadata
		return false;
	}
	
	bool hasSharedContainer()
	{
		// TODO: implement
		return false;
	}
	
	bool hasEditor()
	{
		// TODO: implement. By metadata
		return false;
	}

	const char* getFilePath()
	{
		// TODO: implement
		return NULL;
	}
	
	bool isOutProcess()
	{
		return is_out_process;
	}
};

class AAPEditor
{
	AAPInstance *owner;

public:

	AAPEditor(AAPInstance *owner)
		: owner(owner)
	{
		// TODO: implement
	}

	void startEditorUI()
	{
		// TODO: implement
	}
};

class AAPHost
{
	AAssetManager *asset_manager;
	const char* default_plugin_search_paths[2];
	AAPDescriptor** plugin_descriptors;
	
	AAPDescriptor* loadDescriptorFromAssetBundleDirectory(const char *directory);
	AAPInstance* instantiateLocalPlugin(AAPDescriptor *descriptor);
	AAPInstance* instantiateRemotePlugin(AAPDescriptor *descriptor);

public:

	AAPHost()
		: asset_manager(NULL),
		  plugin_descriptors(NULL)
	{
		default_plugin_search_paths[0] = "/";
		default_plugin_search_paths[1] = NULL;
	}

	~AAPHost()
	{
		if (plugin_descriptors != NULL)
			for (int i = 0; i < getNumPluginDescriptors(); i++)
				free(plugin_descriptors[i]);
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
	
	int32_t getNumPluginDescriptors()
	{
		_AAP_NULL_TERMINATED_LIST(plugin_descriptors)
	}

	AAPDescriptor* getPluginDescriptorAt(int index)
	{
		assert(index < getNumPluginDescriptors());
		return plugin_descriptors[index];
	}
	
	AAPDescriptor* getPluginDescriptor(const char *identifier)
	{
		assert(plugin_descriptors != NULL);
		for(int i = 0; i < getNumPluginDescriptors(); i++) {
			auto d = getPluginDescriptorAt(i);
			if (strcmp(d->getIdentifier(), identifier) == 0)
				return d;
		}
		return NULL;
	}
	
	AAPInstance* instantiatePlugin(const char* identifier);
	
	AAPEditor* createEditor(AAPInstance* instance)
	{
		return new AAPEditor(instance);
	}
};

class AAPInstance
{
	friend class AAPHost;
	
	AAPHost *host;
	AAPDescriptor *descriptor;
	AndroidAudioPlugin *plugin;
	AAPHandle *instance;
	const AndroidAudioPluginExtension * const *extensions;
	PluginInstantiationState plugin_state;

	AAPInstance(AAPHost* host, AAPDescriptor* pluginDescriptor, AndroidAudioPlugin* loadedPlugin)
		: host(host),
		  descriptor(pluginDescriptor),
		  plugin(loadedPlugin),
		  instance(NULL),
		  extensions(NULL),
		  plugin_state(PLUGIN_INSTANTIATION_STATE_UNPREPARED)
	{
	}
	
public:

	AAPDescriptor* getPluginDescriptor()
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
		plugin->process(instance, buffer, timeoutInNanoseconds);
	}
	
	double getTailLengthSeconds() const
	{
		// TODO: implement
		return 0;
	}
	
	AAPEditor* createEditor()
	{
		host->createEditor(this);
	}
	
	int getNumPrograms()
	{
		// TODO: implement
		return 0;
	}
	
	int getCurrentProgram()
	{
		// TODO: implement
		return 0;
	}
	
	void setCurrentProgram(int index)
	{
		// TODO: implement
	}
	
	const char * getProgramName(int index)
	{
		// TODO: implement
		return NULL;
	}
	
	void changeProgramName(int index, const char * newName)
	{
		// TODO: implement
	}
	
	int32_t getStateSize()
	{
		// TODO: implement
		return 0;
	}
	
	void const* getState()
	{
		// TODO: implement
		return NULL;
	}
	
	void setState(const void* data, int32_t offset, int32_t sizeInBytes)
	{
		// TODO: implement
	}
	
	uint32_t getTailTimeInMilliseconds()
	{
		// TODO: implement
		return 0;
	}
};

} // namespace
