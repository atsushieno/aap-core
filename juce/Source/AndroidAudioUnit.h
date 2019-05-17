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
	}
	
	int32_t numInputChannels()
	{
	}
	
	int32_t numOutputChannels()
	{
	}

	bool hasSharedContainer()
	{
	}
	
	bool hasEditor()
	{
	}
	
	bool canProcessMidi()
	{
	}
	
	bool canProduceMidi()
	{
	}
};

class AndroidAudioPluginManager
{
	AAssetManager *asset_manager;
	const char **search_paths;
	bool search_recursive;
	bool asynchronous_instantiation_allowed;
		
public:

	AndroidAudioPluginManager(AAssetManager *assetManager)
		: asset_manager(assetManager),
		  search_paths(NULL),
		  search_recursive(false),
		  asynchronous_instantiation_allowed(false)
	{
	}

	char** getDefaultPluginSearchPaths()
	{
		// TODO: implement
	}

	AndroidAudioPluginDescriptor* getPluginDescriptorList()
	{
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
	}

	void startEditorUI()
	{
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
	
	void processBlock(void **audioBuffer, int audioBufferSize, void **controlBuffers, int controlBufferSize)
	{
		// TODO: implement
	}
	
	double getTailLengthSeconds() const
	{
		// TODO: implement
	}
	
	bool acceptsMidi() const
	{
		// TODO: implement
	}
	
	bool producesMidi() const
	{
		// TODO: implement
	}
	
	AndroidAudioPluginEditor* createEditor()
	{
		return new AndroidAudioPluginEditor(this);
	}
	
	bool hasEditor() const
	{
		// TODO: implement
	}
	
	int getNumPrograms()
	{
		// TODO: implement
	}
	
	int getCurrentProgram()
	{
		// TODO: implement
	}
	
	void setCurrentProgram(int index)
	{
		// TODO: implement
	}
	
	const char * getProgramName(int index)
	{
		// TODO: implement
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
