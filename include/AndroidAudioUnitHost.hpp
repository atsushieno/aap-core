/*
  ==============================================================================

    AndroidAudioUnit.h
    Created: 9 May 2019 3:13:10am
    Author:  atsushieno

  ==============================================================================
*/

#pragma once

#include <dlfcn.h>
#include "../JuceLibraryCode/JuceHeader.h"
#include "android/asset_manager.h"
#include "AndroidAudioUnit.h"


namespace aap
{

class AndroidAudioPluginInstance;

enum AAPBufferType { AAP_BUFFER_TYPE_AUDIO, AAP_BUFFER_TYPE_CONTROL };
enum AAPPortDirection { AAP_PORT_DIRECTION_INPUT, AAP_PORT_DIRECTION_OUTPUT };
enum PluginInstanceState {
	PLUGIN_INSTANCE_UNPREPARED,
	PLUGIN_INSTANCE_INACTIVE,
	PLUGIN_INSTANCE_ACTIVE,
	PLUGIN_INSTANCE_TERMINATED,
};

class AndroidAudioPluginPort
{
	const char *name;
	const AAPBufferType buffer_type;
	const AAPPortDirection direction;
	
public:
	const char* getName() { return name; }
	const AAPBufferType getBufferType() { return buffer_type; }
	const AAPPortDirection getPortDirection() { return direction; }
};

class AndroidAudioPluginDescriptor
{
	const char *name;
	const char *display_name;
	const char * const *categories;
	int num_categories;
	const char *manufacturer_name;
	const char *version;
	const char *identifier_string;
	int unique_id;
	long last_updated_unixtime;

	AndroidAudioPluginPort **ports;
	int num_ports;

	// hosting information
	bool is_out_process;

public:
	const char* getName()
	{
		return name;
	}
	
	const char* getDescriptiveName()
	{
		return display_name;
	}
	
	int32_t numCategories()
	{
		return num_categories;
	}
	
	const char* getCategoryAt(int32_t index)
	{
		return categories [index];
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
	
	int getNumPorts()
	{
		return num_ports;
	}
	
	AndroidAudioPluginPort *getPort(int index)
	{
		assert(index < num_ports);
		return ports[index];
	}
	
	long getFileModTime()
	{
		// TODO: implement
		return 0;
	}
	
	long getLastInfoUpdateTime()
	{
		return last_updated_unixtime;
	}
	
	int32_t getUid()
	{
		return unique_id;
	}
	
	bool isInstrument()
	{
		// TODO: implement
		return false;
	}
	
	bool hasSharedContainer()
	{
		// TODO: implement
		return false;
	}
	
	bool hasEditor()
	{
		// TODO: implement
		return false;
	}
	
	bool hasMidiInputPort()
	{
		// TODO: implement
		return false;
	}
	
	bool hasMidiOutputPort()
	{
		// TODO: implement
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

class AndroidAudioPluginManager
{
	AAssetManager *asset_manager;
	const char* default_plugin_search_paths[2];
	AndroidAudioPluginDescriptor** plugin_descriptors;
	
	AndroidAudioPluginDescriptor* loadDescriptorFromBundleDirectory(const char *directory);
	AndroidAudioPluginInstance* instantiateLocalPlugin(AndroidAudioPluginDescriptor *descriptor);
	AndroidAudioPluginInstance* instantiateRemotePlugin(AndroidAudioPluginDescriptor *descriptor);

public:

	AndroidAudioPluginManager()
		: asset_manager(NULL),
		  plugin_descriptors(NULL)
	{
		default_plugin_search_paths[0] = "~/.aap";
		default_plugin_search_paths[1] = NULL;
	}
	
	void initialize(AAssetManager *assetManager)
	{
		asset_manager = assetManager;
	}

	bool isPluginAlive (const char *identifier);
	
	bool isPluginUpToDate (const char *identifier, long lastInfoUpdated);

	const char** getDefaultPluginSearchPaths()
	{
		return default_plugin_search_paths;
	}

	AndroidAudioPluginDescriptor** getPluginDescriptorList()
	{
		return plugin_descriptors;
	}
	
	AndroidAudioPluginDescriptor* getPluginDescriptor(const char *identifier)
	{
		// TODO: implement
		return NULL;
	}
	
	void updatePluginDescriptorList(const char **searchPaths, bool recursive, bool asynchronousInstantiationAllowed);
	
	AndroidAudioPluginInstance* instantiatePlugin(AndroidAudioPluginDescriptor *descriptor);
};

class AndroidAudioPluginEditor
{
	AndroidAudioPluginInstance *owner;

public:

	AndroidAudioPluginEditor(AndroidAudioPluginInstance *owner)
		: owner(owner)
	{
		// TODO: implement
	}

	void startEditorUI()
	{
		// TODO: implement
	}
};

class AndroidAudioPluginInstance
{
	friend class AndroidAudioPluginManager;
	
	AndroidAudioPluginDescriptor *descriptor;
	AndroidAudioPlugin *plugin;
	AAPHandle *instance;
	const AndroidAudioPluginExtension * const *extensions;
	PluginInstanceState plugin_state;

	AndroidAudioPluginInstance(AndroidAudioPluginDescriptor* pluginDescriptor, AndroidAudioPlugin* loadedPlugin)
		: descriptor(pluginDescriptor),
		  plugin(loadedPlugin),
		  instance(NULL),
		  extensions(NULL),
		  plugin_state(PLUGIN_INSTANCE_UNPREPARED)
	{
	}
	
public:

	AndroidAudioPluginDescriptor* getPluginDescriptor()
	{
		return descriptor;
	}

	void prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock)
	{
		// FIXME: pass buffer hint for LV2 bridges, taking maximumExpectedSamplesPerBlock into consideration.
		instance = plugin->instantiate(plugin, sampleRate, extensions);
		plugin->prepare(instance);
		plugin_state = PLUGIN_INSTANCE_INACTIVE;
	}
	
	void dispose()
	{
		if (instance != NULL)
			plugin->terminate(instance);
		instance = NULL;
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
	
	AndroidAudioPluginEditor* createEditor()
	{
		return new AndroidAudioPluginEditor(this);
	}
	
	bool hasEditor() const
	{
		// TODO: implement
		return false;
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
