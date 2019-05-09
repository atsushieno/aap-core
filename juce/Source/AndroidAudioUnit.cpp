/*
  ==============================================================================

    AndroidAudioUnit.cpp
    Created: 9 May 2019 3:09:22am
    Author:  atsushi

  ==============================================================================
*/

#include "AndroidAudioUnit.h"

using namespace juce;


class AndroidAudioUnitPluginInstance : public juce::AudioPluginInstance
{
public:
	const String getName() const
	{
		// TODO: implement
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


AndroidAudioUnitPluginInstance p;
AndroidAudioUnitPluginFormat f;
