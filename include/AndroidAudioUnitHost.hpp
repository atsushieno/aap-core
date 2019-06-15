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



class AndroidAudioPluginDescriptor
{
	char *name;
	char *display_name;
	char **categories;
	int num_categories;
	char *manufacturer_name;
	char *version;
	char *identifier_string;
	int unique_id;
	long last_updated_unixtime;

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
	
	int32_t numInputChannels()
	{
		// TODO: implement
		return 0;
	}
	
	int32_t numOutputChannels()
	{
		// TODO: implement
		return 0;
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

	AndroidAudioPluginInstance(AndroidAudioPluginDescriptor* pluginDescriptor, AndroidAudioPlugin* loadedPlugin)
		: descriptor(pluginDescriptor),
		  plugin(loadedPlugin),
		  instance(NULL),
		  extensions(NULL)
	{
	}
	
public:

	AndroidAudioPluginDescriptor* getPluginDescriptor()
	{
		return descriptor;
	}

	void prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock)
	{
		// FIXME: determine how to deal with maximumExpectedSamplesPerBlock (process or ignore)
		instance = plugin->instantiate(plugin, sampleRate, extensions);
		plugin->prepare(instance);
	}
	
	void dispose()
	{
		if (instance != NULL)
			plugin->terminate(instance);
		instance = NULL;
	}
	
	void process(AndroidAudioPluginBuffer *audioBuffer, AndroidAudioPluginBuffer *controlBuffers, int32_t timeoutInNanoseconds)
	{
		plugin->process(instance, audioBuffer, controlBuffers, timeoutInNanoseconds);
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
