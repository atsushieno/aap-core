
#include "../JuceLibraryCode/JuceHeader.h"
#include "../../plugin-api/include/aap/android-audio-plugin.h"

extern juce::AudioProcessor* createPluginFilter(); // it is defined in each Audio plugin project (by Projucer).

typedef class JuceAAPWrapper {
	AndroidAudioPluginFactory *factory;
	const char* plugin_unique_id;
	int sample_rate;
	const AndroidAudioPluginExtension * const *extensions;
	AndroidAudioPluginBuffer *buffer;
	juce::AudioProcessor *juce_processor;
	juce::AudioBuffer<float> juce_buffer;
	juce::MidiBuffer juce_midi_messages;

public:
	JuceAAPWrapper(AndroidAudioPluginFactory *factory, const char* pluginUniqueId, int sampleRate, const AndroidAudioPluginExtension * const *extensions)
		: factory(factory), plugin_unique_id(pluginUniqueId == nullptr ? nullptr : strdup(pluginUniqueId)), sample_rate(sampleRate), extensions(extensions)
	{
        juce_processor = createPluginFilter();
	}

	virtual ~JuceAAPWrapper()
	{
		if (plugin_unique_id != nullptr)
			free((void*) plugin_unique_id);
	}

	void prepare(AndroidAudioPluginBuffer* buffer)
	{
        // FIXME: allocate juce_buffer. No need to interpret content.
		this->buffer = buffer;
        juce_processor->prepareToPlay(sample_rate, buffer->num_frames);
	}

	void activate()
	{
	    // FIXME: what can I do?
	}

	void deactivate()
	{
        // FIXME: what can I do?
	}

	void process(AndroidAudioPluginBuffer* audioBuffer, long timeoutInNanoseconds)
	{
        // FIXME: reallocate juce_buffer, if pointer changed.
        // FIXME: implement content convert.
        juce_processor->processBlock(juce_buffer, juce_midi_messages);
	}

	// FIXME: we should probably make changes to getState() signature to not return raw pointer,
	//  as it expects that memory allocation runs at plugins. It should be passed by argument and
	//  allocated externally. JUCE does it better.
	AndroidAudioPluginState* getState()
	{
	    MemoryBlock mb;
	    mb.reset();
	    juce_processor->getStateInformation(mb);
	}

	void setState(AndroidAudioPluginState* input)
	{
	    juce_processor->setStateInformation(input->raw_data, input->data_size);
	}
};

JuceAAPWrapper* getWrapper(AndroidAudioPlugin* plugin)
{
	return (JuceAAPWrapper*) plugin->plugin_specific;
}

void juceaap_prepare(
	AndroidAudioPlugin *plugin,
	AndroidAudioPluginBuffer* audioBuffer)
{
	getWrapper(plugin)->prepare(audioBuffer);
}

void juceaap_activate(AndroidAudioPlugin *plugin)
{
	getWrapper(plugin)->activate();
}

void juceaap_deactivate(AndroidAudioPlugin *plugin)
{
	getWrapper(plugin)->deactivate();
}

void juceaap_process(
	AndroidAudioPlugin *plugin,
	AndroidAudioPluginBuffer* audioBuffer,
	long timeoutInNanoseconds)
{
	getWrapper(plugin)->process(audioBuffer, timeoutInNanoseconds);
}

const AndroidAudioPluginState* juceaap_get_state(AndroidAudioPlugin *plugin)
{
	return getWrapper(plugin)->getState();
}

void juceaap_set_state(AndroidAudioPlugin *plugin, AndroidAudioPluginState *input)
{
	getWrapper(plugin)->setState(input);
}

AndroidAudioPlugin* juceaap_instantiate(
	AndroidAudioPluginFactory *pluginFactory,
	const char* pluginUniqueId,
	int sampleRate,
	const AndroidAudioPluginExtension * const *extensions)
{
	auto *ret = new AndroidAudioPlugin();
	auto *ctx = new JuceAAPWrapper(pluginFactory, pluginUniqueId, sampleRate, extensions);

	ret->plugin_specific = ctx;

	ret->prepare = juceaap_prepare;
	ret->activate = juceaap_activate;
	ret->process = juceaap_process;
	ret->deactivate = juceaap_deactivate;
	ret->get_state = juceaap_get_state;
	ret->set_state = juceaap_set_state;

	return ret;
}

void juceaap_release(
	AndroidAudioPluginFactory *pluginFactory,
	AndroidAudioPlugin *instance)
{
	auto ctx = (JuceAAPWrapper*) instance->plugin_specific;
	if (ctx != nullptr) {
		delete ctx;
		instance->plugin_specific = nullptr;
	}
	delete instance;
}

struct AndroidAudioPluginFactory juceaap_factory{
	juceaap_instantiate,
	juceaap_release
};

extern "C" AndroidAudioPluginFactory* GetJuceAAPFactory()
{
  return &juceaap_factory;
}
