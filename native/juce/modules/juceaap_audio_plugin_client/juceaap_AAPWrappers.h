
#include "../JuceLibraryCode/JuceHeader.h"
#include "../../plugin-api/include/aap/android-audio-plugin.h"

typedef class JuceAAPWrapper : public juce::AudioPluginInstance {
	AndroidAudioPluginFactory *factory;
	const char* pluginuniqueId;
	int sampleRate;
	const AndroidAudioPluginExtension * const *extensions;
	AndroidAudioPluginBuffer *buffer;

public:
	JuceAAPWrapper(AndroidAudioPluginFactory *factory, const char* pluginuniqueId, int sampleRate, const AndroidAudioPluginExtension * const *extensions)
		: factory(factory), pluginUniqueId(pluginUniqueId == nullptr ? nullptr : strdup(pluginUniqueId)), sampleRate(sampleRate), extensions(extensions)
	{
	}

	void prepare(AndroidAudioPluginBuffer* buffer)
	{
		this->buffer = buffer;
	}

	void activate()
	{
	}

	void deactivate()
	{
	}

	void process(AndroidAudioPluginBuffer* audioBuffer, long timeoutInNanoseconds)
	{
	}

	AndroidAudioPluginState* getState()
	{
	}

	void setState(AndroidAudioPluginState* input)
	{
	}
}

JuceAAPWrapper* getWrapper(AndroidAudioPlugin* plugin)
{
	return (JuceAAPWrapper*) plugin->pluginSpecific;
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

AndroidAudioPluginState* juceaap_get_state(AndroidAudioPlugin *plugin)
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
	auto ctx = (JuceAAPSpecific*) instance->plugin_specific;
	if (ctx != nullptr) {
		if (ctx->pluginUniqueId != nullptr)
			free(ctx->pluginUniqueId);
		delete ctx;
		instance->plugin_specific = nullptr;
	}
	delete instance;
}

struct JuceAAPAudioPluginFactory juceaap_factory {
	juceaap_instantiate,
	juceaap_release
};

extern "C" AndroidAudioPluginFactory* GetJuceAAPFactory()
{
  return &juceaap_factory;
}
