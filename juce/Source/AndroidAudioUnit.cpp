/*
  ==============================================================================

    AndroidAudioUnit.cpp
    Created: 9 May 2019 3:09:22am
    Author:  atsushi

  ==============================================================================
*/

#include "AndroidAudioUnit.h"

using namespace juce;

/* our framework */

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
	char* getName()
	{
		return name;
	}
	
	char* getDescriptiveName()
	{
		return display_name;
	}
	
	int32_t numCategories()
	{
		return num_categories;
	}
	
	char* getCategoryAt (int32_t index)
	{
		return categories [index];
	}
	
	char* getManufacturerName()
	{
		return manufacturer_name;
	}
	
	char* getVersion()
	{
		return version;
	}
	
	char* getIdentifier()
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
};

class AndroidAudioPlugin
{
	AndroidAudioPluginDescriptor *descriptor;

public:
	AndroidAudioPlugin (AndroidAudioPluginDescriptor *pluginDescriptor)
		: descriptor (pluginDescriptor)
	{
	}

	AndroidAudioPluginDescriptor* getPluginDescriptor ()
	{
		return descriptor;
	}

	void prepareToPlay (double sampleRate, int maximumExpectedSamplesPerBlock)
	{
		// TODO: implement
	}
	
	void releaseResources()
	{
		// TODO: implement
	}
	
	void processBlock (void *audioBuffer, int audioBufferSize, MidiBuffer& midiMessages)
	{
		// TODO: implement
	}
	
	double getTailLengthSeconds () const
	{
		// TODO: implement
	}
	
	bool acceptsMidi () const
	{
		// TODO: implement
	}
	
	bool producesMidi () const
	{
		// TODO: implement
	}
	
	AudioProcessorEditor* createEditor ()
	{
		// TODO: implement
	}
	
	bool hasEditor () const
	{
		// TODO: implement
	}
	
	int getNumPrograms ()
	{
		// TODO: implement
	}
	
	int getCurrentProgram ()
	{
		// TODO: implement
	}
	
	void setCurrentProgram (int index)
	{
		// TODO: implement
	}
	
	const String getProgramName (int index)
	{
		// TODO: implement
	}
	
	void changeProgramName (int index, const String& newName)
	{
		// TODO: implement
	}
	
	void getStateInformation (juce::MemoryBlock& destData)
	{
		// TODO: implement
	}
	
	void setStateInformation (const void* data, int sizeInBytes)
	{
		// TODO: implement
	}
	
	 void fillInPluginDescription (PluginDescription &description) const
	 {
		// TODO: implement
	 }
};


/* juce facades */

class AndroidAudioUnitPluginInstance : public juce::AudioPluginInstance
{
	AndroidAudioPlugin *native;

		
public:

	AndroidAudioUnitPluginInstance (AndroidAudioPlugin *nativePlugin)
		: native (nativePlugin)
	{
	}

	const String getName() const
	{
		return native->getPluginDescriptor()->getName();
	}
	
	void prepareToPlay (double sampleRate, int maximumExpectedSamplesPerBlock)
	{
		// TODO: implement
	}
	
	void releaseResources()
	{
		// TODO: implement
	}
	
	void processBlock (AudioBuffer<float>& buffer, MidiBuffer& midiMessages)
	{
		// TODO: implement
	}
	
	double getTailLengthSeconds () const
	{
		// TODO: implement
	}
	
	bool acceptsMidi () const
	{
		// TODO: implement
	}
	
	bool producesMidi () const
	{
		// TODO: implement
	}
	
	AudioProcessorEditor* createEditor ()
	{
		// TODO: implement
	}
	
	bool hasEditor () const
	{
		// TODO: implement
	}
	
	int getNumPrograms ()
	{
		// TODO: implement
	}
	
	int getCurrentProgram ()
	{
		// TODO: implement
	}
	
	void setCurrentProgram (int index)
	{
		// TODO: implement
	}
	
	const String getProgramName (int index)
	{
		// TODO: implement
	}
	
	void changeProgramName (int index, const String& newName)
	{
		// TODO: implement
	}
	
	void getStateInformation (juce::MemoryBlock& destData)
	{
		// TODO: implement
	}
	
	void setStateInformation (const void* data, int sizeInBytes)
	{
		// TODO: implement
	}
	
	 void fillInPluginDescription (PluginDescription &description) const
	 {
		// TODO: implement
	 }
};

class AndroidAudioUnitPluginFormat : public juce::AudioPluginFormat
{
public:
	String getName () const
	{
		// TODO: implement
	}
	
	void findAllTypesForFile (OwnedArray<PluginDescription>& results, const String& fileOrIdentifier)
	{
		// TODO: implement
	}
	
	bool fileMightContainThisPluginType (const String &fileOrIdentifier)
	{
		// TODO: implement
	}
	
	String getNameOfPluginFromIdentifier (const String &fileOrIdentifier)
	{
		// TODO: implement
	}
	
	bool pluginNeedsRescanning (const PluginDescription &description)
	{
		// TODO: implement
	}
	
	bool doesPluginStillExist (const PluginDescription &description)
	{
		// TODO: implement
	}
	
	bool canScanForPlugins () const
	{
		// TODO: implement
	}
	
	StringArray searchPathsForPlugins (const FileSearchPath &directoriesToSearch,
		bool recursive,
		bool allowPluginsWhichRequireAsynchronousInstantiation = false)
	{
		// TODO: implement
	}
	
	FileSearchPath getDefaultLocationsToSearch ()
	{
		// TODO: implement
	}

protected:
	void createPluginInstance (const PluginDescription &description,
		double initialSampleRate,
		int initialBufferSize,
		void *userData,
		PluginCreationCallback callback)
	{
		// TODO: implement
	}
	
	bool requiresUnblockedMessageThreadDuringCreation (const PluginDescription &description) const noexcept
	{
		// TODO: implement
	}
};


AndroidAudioUnitPluginInstance p (NULL);
AndroidAudioUnitPluginFormat f;
