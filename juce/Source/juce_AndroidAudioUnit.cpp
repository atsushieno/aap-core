/*
  ==============================================================================

    AndroidAudioUnit.cpp
    Created: 9 May 2019 3:09:22am
    Author:  atsushieno

  ==============================================================================
*/

#include "AndroidAudioUnit.h"

using namespace aap;
using namespace juce;


/* juce facades */

class AndroidAudioPluginInstance : public juce::AudioPluginInstance
{
	AndroidAudioPlugin *native;
		
public:

	AndroidAudioPluginInstance(AndroidAudioPlugin *nativePlugin)
		: native(nativePlugin)
	{
	}

	const String getName() const
	{
		return native->getPluginDescriptor()->getName();
	}
	
	void prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock)
	{
		// TODO: implement
	}
	
	void releaseResources()
	{
		native->dispose();
	}
	
	void processBlock(AudioBuffer<float>& buffer, MidiBuffer& midiMessages)
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
	
	AudioProcessorEditor* createEditor()
	{
		// TODO: implement
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
	
	const String getProgramName(int index)
	{
		// TODO: implement
	}
	
	void changeProgramName(int index, const String& newName)
	{
		// TODO: implement
	}
	
	void getStateInformation(juce::MemoryBlock& destData)
	{
		// TODO: implement
	}
	
	void setStateInformation(const void* data, int sizeInBytes)
	{
		// TODO: implement
	}
	
	 void fillInPluginDescription(PluginDescription &description) const
	 {
		AndroidAudioPluginDescriptor *desc = native->getPluginDescriptor();
		description.name = desc->getName();
		description.pluginFormatName = "AAP";
		
		int32_t numCategories = desc->numCategories();
		description.category.clear();
		for (int i = 0; i < numCategories; i++)
			description.category += desc->getCategoryAt(i);
		
		description.manufacturerName = desc->getManufacturerName();
		description.version = desc->getVersion();
		description.fileOrIdentifier = desc->getIdentifier();
		// TODO: fill it
		// description.lastFileModTime
		description.lastInfoUpdateTime = Time(desc->getLastInfoUpdateTime());
		description.uid = desc->getUid();
		description.isInstrument = desc->isInstrument();
		description.numInputChannels = desc->numInputChannels();
		description.numOutputChannels = desc->numOutputChannels();
		description.hasSharedContainer = desc->hasSharedContainer();		
	 }
};

class AndroidAudioPluginFormat : public juce::AudioPluginFormat
{
	AndroidAudioPluginManager android_manager;

public:
	AndroidAudioPluginFormat(AAssetManager *assetManager)
		: android_manager(AndroidAudioPluginManager(assetManager))
	{
	}

	String getName() const
	{
		return "AAP";
	}
	
	void findAllTypesForFile(OwnedArray<PluginDescription>& results, const String& fileOrIdentifier)
	{
		// TODO: implement
	}
	
	bool fileMightContainThisPluginType(const String &fileOrIdentifier)
	{
		// TODO: implement
	}
	
	String getNameOfPluginFromIdentifier(const String &fileOrIdentifier)
	{
		// TODO: implement
	}
	
	bool pluginNeedsRescanning(const PluginDescription &description)
	{
		// TODO: implement
	}
	
	bool doesPluginStillExist(const PluginDescription &description)
	{
		// TODO: implement
	}
	
	bool canScanForPlugins() const
	{
		// TODO: implement
	}
	
	StringArray searchPathsForPlugins(const FileSearchPath &directoriesToSearch,
		bool recursive,
		bool allowPluginsWhichRequireAsynchronousInstantiation = false)
	{
		int numPaths = directoriesToSearch.getNumPaths();
		const char * paths[numPaths];
		for (int i = 0; i < numPaths; i++)
			paths[i] = directoriesToSearch[i].getFullPathName().toRawUTF8();
		android_manager.updatePluginDescriptorList(paths, recursive, allowPluginsWhichRequireAsynchronousInstantiation);
		AndroidAudioPluginDescriptor *results = android_manager.getPluginDescriptorList();
	}
	
	FileSearchPath getDefaultLocationsToSearch()
	{
		char **paths = android_manager.getDefaultPluginSearchPaths();
		StringArray arr(paths);
		String joined = arr.joinIntoString(";");
		FileSearchPath ret(joined);
		return ret;
	}

protected:
	void createPluginInstance(const PluginDescription &description,
		double initialSampleRate,
		int initialBufferSize,
		void *userData,
		PluginCreationCallback callback)
	{
		// TODO: implement
	}
	
	bool requiresUnblockedMessageThreadDuringCreation(const PluginDescription &description) const noexcept
	{
		// TODO: implement
	}
};


AndroidAudioPluginInstance p(NULL);
AndroidAudioPluginFormat f(NULL);
