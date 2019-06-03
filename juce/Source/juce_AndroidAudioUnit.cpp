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

namespace juceaap
{

class AndroidAudioPluginInstance;

class AndroidAudioProcessorEditor : public juce::AudioProcessorEditor
{
	AndroidAudioPluginEditor *native;
	
public:

	AndroidAudioProcessorEditor(AudioProcessor *processor, AndroidAudioPluginEditor *native)
		: AudioProcessorEditor(processor), native(native)
	{
	}
	
	void startEditorUI()
	{
		native->startEditorUI();
	}
	
	// TODO: override if we want to.
	/*
	virtual void setScaleFactor(float newScale)
	{
	}
	*/
};

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
		return native->getPluginDescriptor()->hasMidiInputPort();
	}
	
	bool producesMidi() const
	{
		return native->getPluginDescriptor()->hasMidiOutputPort();
	}
	
	AudioProcessorEditor* createEditor()
	{
		if (!native->getPluginDescriptor()->hasEditor())
			return nullptr;
		auto ret = new AndroidAudioProcessorEditor(this, native->createEditor());
		ret->startEditorUI();
		return ret;
	}
	
	bool hasEditor() const
	{
		return native->hasEditor();
	}
	
	int getNumPrograms()
	{
		return native->getNumPrograms();
	}
	
	int getCurrentProgram()
	{
		return native->getCurrentProgram();
	}
	
	void setCurrentProgram(int index)
	{
		native->setCurrentProgram(index);
	}
	
	const String getProgramName(int index)
	{
		return native->getProgramName(index);
	}
	
	void changeProgramName(int index, const String& newName)
	{
		native->changeProgramName(index, newName.toUTF8());
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
		auto desc = native->getPluginDescriptor();
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

	AndroidAudioPluginDescriptor *findDescriptorFrom(const PluginDescription &desc)
	{
	}

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
		auto results = android_manager.getPluginDescriptorList();
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
		auto descriptor = findDescriptorFrom(description);
		String error("");
		if (descriptor == nullptr) {
			error << "Android Audio Plugin " << description.name << "was not found.";
			callback(userData, nullptr, error);
		} else {
			auto androidInstance = android_manager.instantiatePlugin(descriptor);
			auto instance = new AndroidAudioPluginInstance(androidInstance);
			callback(userData, instance, error);
		}
	}
	
	bool requiresUnblockedMessageThreadDuringCreation(const PluginDescription &description) const noexcept
	{
		// TODO: implement
	}
};


AndroidAudioPluginInstance p(NULL);
AndroidAudioPluginFormat f(NULL);

} // namespace
