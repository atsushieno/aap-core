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

static void fillPluginDescriptionFromNative(PluginDescription &description, AndroidAudioPluginDescriptor &src)
{
	description.name = src.getName();
	description.pluginFormatName = "AAP";
	
	int32_t numCategories = src.numCategories();
	description.category.clear();
	for (int i = 0; i < numCategories; i++)
		description.category += src.getCategoryAt(i);
	
	description.manufacturerName = src.getManufacturerName();
	description.version = src.getVersion();
	description.fileOrIdentifier = src.getIdentifier();
	// TODO: fill it
	// description.lastFileModTime
	description.lastInfoUpdateTime = Time(src.getLastInfoUpdateTime());
	description.uid = src.getUid();
	description.isInstrument = src.isInstrument();
	description.numInputChannels = src.numInputChannels();
	description.numOutputChannels = src.numOutputChannels();
	description.hasSharedContainer = src.hasSharedContainer();		
}

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
	
	AndroidAudioPluginBuffer* toNativeAudioBuffers(AudioBuffer<float>& buffer)
	{
		// TODO: implement
		return NULL;
	}
	
	AndroidAudioPluginBuffer* toNativeMidiBuffers(MidiBuffer& buffer)
	{
		// TODO: implement
		return NULL;
	}
	
	void processBlock(AudioBuffer<float>& buffer, MidiBuffer& midiMessages)
	{
		native->process(toNativeAudioBuffers(buffer), toNativeMidiBuffers(midiMessages), 0);
	}
	
	double getTailLengthSeconds() const
	{
		// TODO: implement
		return 0;
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
		auto src = native->getPluginDescriptor();
		fillPluginDescriptionFromNative(description, *src);
	 }
};

class AndroidAudioPluginFormat : public juce::AudioPluginFormat
{
	AndroidAudioPluginManager android_manager;
	HashMap<AndroidAudioPluginDescriptor*,PluginDescription*> cached_descs;

	AndroidAudioPluginDescriptor *findDescriptorFrom(const PluginDescription &desc)
	{
		// TODO: implement
		return NULL;
	}


public:
	AndroidAudioPluginFormat(AAssetManager *assetManager)
		: android_manager(AndroidAudioPluginManager(assetManager))
	{
	}

	~AndroidAudioPluginFormat()
	{
		// TODO: implement
		// release PluginDescription objects in cached_descs.
	}

	String getName() const
	{
		return "AAP";
	}
	
	void findAllTypesForFile(OwnedArray<PluginDescription>& results, const String& fileOrIdentifier)
	{
		auto id = fileOrIdentifier.toRawUTF8();
		auto descriptor = android_manager.getPluginDescriptorList();
		for (auto d = descriptor; d != NULL; d++) {
			if (strcmp(id, d->getName()) == 0 || strcmp(id, d->getIdentifier()) == 0) {
				auto dst = cached_descs [d];
				if (!dst) {
					dst = new PluginDescription();
					cached_descs.set(d, dst);
					fillPluginDescriptionFromNative(*dst, *d);
				}
				results.add(dst);
			}
		}
	}
	
	bool fileMightContainThisPluginType(const String &fileOrIdentifier)
	{
		auto f = File::createFileWithoutCheckingPath(fileOrIdentifier);
		return f.hasFileExtension(".aap");
	}
	
	String getNameOfPluginFromIdentifier(const String &fileOrIdentifier)
	{
		auto descriptor = android_manager.getPluginDescriptor(fileOrIdentifier.toRawUTF8());
		return descriptor != NULL ? String(descriptor->getName()) : String();
	}
	
	bool pluginNeedsRescanning(const PluginDescription &description)
	{
		return android_manager.isPluginUpToDate (description.fileOrIdentifier.toRawUTF8(), description.lastInfoUpdateTime.toMilliseconds());
	}
	
	bool doesPluginStillExist(const PluginDescription &description)
	{
		return android_manager.isPluginAlive (description.fileOrIdentifier.toRawUTF8());
	}
	
	bool canScanForPlugins() const
	{
		return true;
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
		const char **paths = android_manager.getDefaultPluginSearchPaths();
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
		return false;
	}
};


AndroidAudioPluginInstance p(NULL);
AndroidAudioPluginFormat f(NULL);

} // namespace
