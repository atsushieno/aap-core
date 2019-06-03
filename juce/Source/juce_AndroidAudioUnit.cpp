/*
  ==============================================================================

    juce_AndroidAudioUnit.cpp
    Created: 9 May 2019 3:09:22am
    Author:  atsushieno

  ==============================================================================
*/

#include "AndroidAudioUnit.h"

using namespace aap;
using namespace juce;

namespace juceaap
{

class JuceAndroidAudioPluginInstance;

class JuceAndroidAudioProcessorEditor : public juce::AudioProcessorEditor
{
	AndroidAudioPluginEditor *native;
	
public:

	JuceAndroidAudioProcessorEditor(AudioProcessor *processor, AndroidAudioPluginEditor *native)
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

class JuceAndroidAudioPluginInstance : public juce::AudioPluginInstance
{
	AndroidAudioPluginInstance *native;

public:

	JuceAndroidAudioPluginInstance(AndroidAudioPluginInstance *nativePlugin)
		: native(nativePlugin)
	{
	}

	const String getName() const override
	{
		return native->getPluginDescriptor()->getName();
	}
	
	void prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock) override
	{
		// TODO: implement
	}
	
	void releaseResources() override
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
	
	void processBlock(AudioBuffer<float>& buffer, MidiBuffer& midiMessages) override
	{
		native->process(toNativeAudioBuffers(buffer), toNativeMidiBuffers(midiMessages), 0);
	}
	
	double getTailLengthSeconds() const override
	{
		return native->getTailTimeInMilliseconds();
	}
	
	bool acceptsMidi() const override
	{
		return native->getPluginDescriptor()->hasMidiInputPort();
	}
	
	bool producesMidi() const override
	{
		return native->getPluginDescriptor()->hasMidiOutputPort();
	}
	
	AudioProcessorEditor* createEditor() override
	{
		if (!native->getPluginDescriptor()->hasEditor())
			return nullptr;
		auto ret = new JuceAndroidAudioProcessorEditor(this, native->createEditor());
		ret->startEditorUI();
		return ret;
	}
	
	bool hasEditor() const override
	{
		return native->hasEditor();
	}
	
	int getNumPrograms() override
	{
		return native->getNumPrograms();
	}
	
	int getCurrentProgram() override
	{
		return native->getCurrentProgram();
	}
	
	void setCurrentProgram(int index) override
	{
		native->setCurrentProgram(index);
	}
	
	const String getProgramName(int index) override
	{
		return native->getProgramName(index);
	}
	
	void changeProgramName(int index, const String& newName) override
	{
		native->changeProgramName(index, newName.toUTF8());
	}
	
	void getStateInformation(juce::MemoryBlock& destData) override
	{
		int32_t size = native->getStateSize();
		destData.setSize(size);
		destData.copyFrom(native->getState(), 0, size);
	}
	
	void setStateInformation(const void* data, int sizeInBytes) override
	{
		native->setState(data, 0, sizeInBytes);
	}
	
	 void fillInPluginDescription(PluginDescription &description) const override
	 {
		auto src = native->getPluginDescriptor();
		fillPluginDescriptionFromNative(description, *src);
	 }
};

class JuceAndroidAudioPluginFormat : public juce::AudioPluginFormat
{
	AndroidAudioPluginManager android_manager;
	HashMap<AndroidAudioPluginDescriptor*,PluginDescription*> cached_descs;

	AndroidAudioPluginDescriptor *findDescriptorFrom(const PluginDescription &desc)
	{
		// TODO: implement
		return NULL;
	}


public:
	JuceAndroidAudioPluginFormat(AAssetManager *assetManager)
		: android_manager(AndroidAudioPluginManager(assetManager))
	{
	}

	~JuceAndroidAudioPluginFormat()
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
		// FIXME: implement
		return StringArray();
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
			auto instance = new JuceAndroidAudioPluginInstance(androidInstance);
			callback(userData, instance, error);
		}
	}
	
	bool requiresUnblockedMessageThreadDuringCreation(const PluginDescription &description) const noexcept
	{
		// FIXME: implement correctly(?)
		return false;
	}
};


JuceAndroidAudioPluginInstance p(NULL);
JuceAndroidAudioPluginFormat f(NULL);

} // namespace
