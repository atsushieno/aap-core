
#include "../JuceLibraryCode/JuceHeader.h"
#include "aap/android-audio-plugin-host.hpp"

using namespace aap;
using namespace juce;

namespace juceaap {

class AndroidAudioPluginEditor : public juce::AudioProcessorEditor {
	aap::EditorInstance *native;

public:

    AndroidAudioPluginEditor(juce::AudioProcessor *processor, aap::EditorInstance *native);

	inline void startEditorUI() {
		native->startEditorUI();
	}

	// TODO: FUTURE (v0.6). most likely ignorable as the UI is for Android anyways.
	/*
    virtual void setScaleFactor(float newScale)
    {
    }
    */
};

class AndroidAudioPluginInstance : public juce::AudioPluginInstance {
	aap::PluginInstance *native;
	int sample_rate;
	AndroidAudioPluginBuffer buffer;
	float *dummy_raw_buffer;

	void fillNativeAudioBuffers(AndroidAudioPluginBuffer *dst, AudioBuffer<float> &buffer);

	void fillNativeMidiBuffers(AndroidAudioPluginBuffer *dst, MidiBuffer &buffer, int bufferIndex);

	int getNumBuffers(AndroidAudioPluginBuffer *buffer);

public:

	AndroidAudioPluginInstance(aap::PluginInstance *nativePlugin);

	inline const String getName() const override {
		return native->getPluginDescriptor()->getName();
	}

	void prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock) override;

	void releaseResources() override;

	void processBlock(AudioBuffer<float> &audioBuffer, MidiBuffer &midiMessages) override;

	double getTailLengthSeconds() const override;

	bool hasMidiPort(bool isInput) const;

	inline bool acceptsMidi() const override {
		return hasMidiPort(true);
	}

	inline bool producesMidi() const override {
		return hasMidiPort(false);
	}

	AudioProcessorEditor *createEditor() override;

	inline bool hasEditor() const override {
		return native->getPluginDescriptor()->hasEditor();
	}

	inline int getNumPrograms() override {
		return native->getNumPrograms();
	}

	inline int getCurrentProgram() override {
		return native->getCurrentProgram();
	}

	inline void setCurrentProgram(int index) override {
		native->setCurrentProgram(index);
	}

	inline const String getProgramName(int index) override {
		return native->getProgramName(index);
	}

	inline void changeProgramName(int index, const String &newName) override {
		native->changeProgramName(index, newName.toUTF8());
	}

	inline void getStateInformation(juce::MemoryBlock &destData) override {
		int32_t size = native->getStateSize();
		destData.setSize(size);
		destData.copyFrom(native->getState(), 0, size);
	}

	inline void setStateInformation(const void *data, int sizeInBytes) override {
		native->setState(data, 0, sizeInBytes);
	}

	void fillInPluginDescription(PluginDescription &description) const override;
};

class AndroidAudioPluginFormat : public juce::AudioPluginFormat {
	aap::PluginHost android_host;
	HashMap<const aap::PluginInformation *, PluginDescription *> cached_descs;

	const aap::PluginInformation *findDescriptorFrom(const PluginDescription &desc);

	const char *default_plugin_search_paths[1];

public:
	AndroidAudioPluginFormat(const aap::PluginInformation *const *pluginDescriptors);

	~AndroidAudioPluginFormat();

	inline String getName() const override {
		return "AAP";
	}

	void findAllTypesForFile(OwnedArray <PluginDescription> &results,
							 const String &fileOrIdentifier) override;

	inline bool fileMightContainThisPluginType(const String &fileOrIdentifier) override {
		auto f = File::createFileWithoutCheckingPath(fileOrIdentifier);
		return f.hasFileExtension(".aap");
	}

	inline String getNameOfPluginFromIdentifier(const String &fileOrIdentifier) override {
		auto descriptor = android_host.getPluginDescriptor(fileOrIdentifier.toRawUTF8());
		return descriptor != NULL ? String(descriptor->getName()) : String();
	}

	inline bool pluginNeedsRescanning(const PluginDescription &description) override {
		return android_host.isPluginUpToDate(description.fileOrIdentifier.toRawUTF8(),
											 description.lastInfoUpdateTime.toMilliseconds());
	}

	inline bool doesPluginStillExist(const PluginDescription &description) override {
		return android_host.isPluginAlive(description.fileOrIdentifier.toRawUTF8());
	}

	inline bool canScanForPlugins() const override {
		return true;
	}

	inline StringArray searchPathsForPlugins(const FileSearchPath &directoriesToSearch,
									  bool recursive,
									  bool allowPluginsWhichRequireAsynchronousInstantiation = false) override {
		// regardless of whatever parameters this function is passed, it is
		// impossible to change the list of detected plugins.
		StringArray ret;
		for (int i = 0; i < android_host.getNumPluginDescriptors(); i++)
			ret.add(android_host.getPluginDescriptorAt(i)->getIdentifier());
		return ret;
	}

	// Unlike desktop system, it is not practical to either look into file systems
	// on Android. And it is simply impossible to "enumerate" asset directories.
	// Therefore we simply return empty list.
	inline FileSearchPath getDefaultLocationsToSearch() override {
		FileSearchPath ret("");
		return ret;
	}

	inline bool isTrivialToScan() const override { return true; }

	inline void createPluginInstance(const PluginDescription &description,
							  double initialSampleRate,
							  int initialBufferSize,
							  PluginCreationCallback callback) override;

protected:
	inline bool requiresUnblockedMessageThreadDuringCreation(
			const PluginDescription &description) const noexcept override {
		// FIXME: implement correctly(?)
		return false;
	}
};

} // namespace
