
#include "../JuceLibraryCode/JuceHeader.h"
#include "aap/android-audio-plugin-host.hpp"

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

class AndroidAudioPluginParameter;

class AndroidAudioPluginInstance : public juce::AudioPluginInstance {
    friend class AndroidAudioPluginParameter;

	aap::PluginInstance *native;
	int sample_rate;
	std::unique_ptr<AndroidAudioPluginBuffer> buffer{nullptr};
	std::map<int32_t,int32_t> portMapAapToJuce{};

	void fillNativeInputBuffers(AudioBuffer<float> &audioBuffer, MidiBuffer &midiBuffer);
	void fillNativeOutputBuffers(AudioBuffer<float> &buffer);

    void allocateSharedMemory(int bufferIndex, int32_t size);

    void updateParameterValue(AndroidAudioPluginParameter* paramter);

public:

	AndroidAudioPluginInstance(aap::PluginInstance *nativePlugin);
	~AndroidAudioPluginInstance() {
		delete buffer->buffers;
	}
	void destroyResources();

	inline const String getName() const override {
		return native->getPluginInformation()->getDisplayName();
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
		return native->getPluginInformation()->hasEditor();
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
		auto state = native->getState();
		destData.setSize(state.data_size);
		destData.copyFrom(state.raw_data, 0, state.data_size);
	}

	inline void setStateInformation(const void *data, int sizeInBytes) override {
		native->setState(data, sizeInBytes);
	}

	void fillInPluginDescription(PluginDescription &description) const override;
};

class AndroidAudioPluginParameter : public juce::AudioProcessorParameter {
    friend class AndroidAudioPluginInstance;

    int aap_parameter_index;
    AndroidAudioPluginInstance *instance;
    const aap::PortInformation *impl;

    AndroidAudioPluginParameter(int aapParameterIndex, AndroidAudioPluginInstance* instance, const aap::PortInformation* portInfo)
            : aap_parameter_index(aapParameterIndex), instance(instance), impl(portInfo)
    {
        if (portInfo->hasValueRange())
            setValue(portInfo->getDefaultValue());
    }

    float value{0.0f};

public:
    int getAAPParameterIndex() { return aap_parameter_index; }

    float getValue() const override { return value; }

    void setValue(float newValue) override {
        value = newValue;
        instance->updateParameterValue(this);
    }

    float getDefaultValue() const override {
        return impl->getDefaultValue();
    }

    String getName(int maximumStringLength) const override {
        String name{impl->getName()};
        return (name.length() <= maximumStringLength) ? name : name.substring(0, maximumStringLength);
    }

    String getLabel() const override { return impl->getName(); }

    float getValueForText(const String &text) const override { return atof(text.toRawUTF8()); }
};

class AndroidAudioPluginFormat : public juce::AudioPluginFormat {
	aap::PluginHostManager android_host_manager;
	aap::PluginHost android_host;
	OwnedArray<PluginDescription> juce_plugin_descs;
	HashMap<const aap::PluginInformation *, PluginDescription *> cached_descs;

	const aap::PluginInformation *findPluginInformationFrom(const PluginDescription &desc);

public:
	AndroidAudioPluginFormat();

	~AndroidAudioPluginFormat();

	inline String getName() const override {
		return "AAP";
	}

	void findAllTypesForFile(OwnedArray <PluginDescription> &results,
							 const String &fileOrIdentifier) override;

	inline bool fileMightContainThisPluginType(const String &fileOrIdentifier) override {
	    // FIXME: limit to aap_metadata.xml ?
		return true;
	}

	inline String getNameOfPluginFromIdentifier(const String &fileOrIdentifier) override {
		auto pluginInfo = android_host_manager.getPluginInformation(fileOrIdentifier.toRawUTF8());
		return pluginInfo != NULL ? String(pluginInfo->getDisplayName()) : String();
	}

	inline bool pluginNeedsRescanning(const PluginDescription &description) override {
		return android_host_manager.isPluginUpToDate(description.fileOrIdentifier.toRawUTF8(),
											 description.lastInfoUpdateTime.toMilliseconds());
	}

	inline bool doesPluginStillExist(const PluginDescription &description) override {
		return android_host_manager.isPluginAlive(description.fileOrIdentifier.toRawUTF8());
	}

	inline bool canScanForPlugins() const override {
		return true;
	}

	StringArray searchPathsForPlugins(const FileSearchPath &directoriesToSearch,
									  bool recursive,
									  bool allowPluginsWhichRequireAsynchronousInstantiation = false) override;

	// Note:
	// Unlike desktop system, it is not practical to either look into file systems
	// on Android. And it is simply impossible to "enumerate" asset directories.
	// Therefore we simply return empty list.
	FileSearchPath getDefaultLocationsToSearch() override;

	inline bool isTrivialToScan() const override { return false; }

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
