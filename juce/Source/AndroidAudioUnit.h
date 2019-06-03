/*
  ==============================================================================

    AndroidAudioUnit.h
    Created: 9 May 2019 3:13:10am
    Author:  atsushieno

  ==============================================================================
*/

#pragma once

#include "../JuceLibraryCode/JuceHeader.h"
#include "android/asset_manager.h"


extern "C" {

//typedef int32_t (*android_audio_plugin_processor_func_t) (AndroidAudioPluginBuffer *audioBuffer, AndroidAudioPluginBuffer *controlBuffer, 	int64_t timeoutInNanoseconds);

typedef struct {
	void **buffers;
	int32_t numBuffers;
	int32_t numFrames;
} AndroidAudioPluginBuffer;


int32_t android_audio_plugin_buffer_num_buffers (AndroidAudioPluginBuffer *buffer);

void **android_audio_plugin_buffer_get_buffers (AndroidAudioPluginBuffer *buffer);

int32_t android_audio_plugin_buffer_num_frames (AndroidAudioPluginBuffer *buffer);

} // extern "C"


namespace aap
{

class AndroidAudioPlugin;



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
};

class AndroidAudioPluginManager
{
	AAssetManager *asset_manager;
	const char **search_paths;
	bool search_recursive;
	bool asynchronous_instantiation_allowed;
	const char* default_plugin_search_paths[1];
		
public:

	AndroidAudioPluginManager(AAssetManager *assetManager)
		: asset_manager(assetManager),
		  search_paths(NULL),
		  search_recursive(false),
		  asynchronous_instantiation_allowed(false)
	{
		// TODO: implement
	}

	bool isPluginAlive (const char *identifier)
	{
		// TODO: implement
		return false;
	}
	
	bool isPluginUpToDate (const char *identifier, long lastInfoUpdated)
	{
		// TODO: implement
		return false;
	}

	const char** getDefaultPluginSearchPaths()
	{
		default_plugin_search_paths[0] = "~/.app";
		return default_plugin_search_paths;
	}

	AndroidAudioPluginDescriptor* getPluginDescriptorList()
	{
		// TODO: implement
		return NULL;
	}
	
	AndroidAudioPluginDescriptor* getPluginDescriptor(const char *identifier)
	{
		// TODO: implement
		return NULL;
	}
	
	void updatePluginDescriptorList(const char **searchPaths, bool recursive, bool asynchronousInstantiationAllowed)
	{
		search_paths = searchPaths;
		search_recursive = recursive;
		asynchronous_instantiation_allowed = asynchronousInstantiationAllowed;
	}
	
	AndroidAudioPlugin* instantiatePlugin(AndroidAudioPluginDescriptor *descriptor);
};

class AndroidAudioPluginEditor
{
	AndroidAudioPlugin *owner;

public:

	AndroidAudioPluginEditor(AndroidAudioPlugin *owner)
		: owner(owner)
	{
		// TODO: implement
	}

	void startEditorUI()
	{
		// TODO: implement
	}
};

class AndroidAudioPlugin
{
	friend class AndroidAudioPluginManager;
	
	AndroidAudioPluginDescriptor *descriptor;

	AndroidAudioPlugin(AndroidAudioPluginDescriptor *pluginDescriptor)
		: descriptor(pluginDescriptor)
	{
	}

public:

	AndroidAudioPluginDescriptor* getPluginDescriptor()
	{
		return descriptor;
	}

	void prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock)
	{
		// TODO: implement
	}
	
	void dispose()
	{
		// TODO: implement
	}
	
	void process(AndroidAudioPluginBuffer *audioBuffer, AndroidAudioPluginBuffer *controlBuffers, int32_t timeoutInNanoseconds)
	{
		// TODO: implement
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
	
	void getStateInformation(void const * destData)
	{
		// TODO: implement
	}
	
	void setStateInformation(const void* data, int sizeInBytes)
	{
		// TODO: implement
	}
};

} // namespace
