
#include "aap/android-audio-plugin-host.hpp"

namespace dogfoodingaap {

void sample_plugin_delete(
	AndroidAudioPluginFactory *pluginFactory,
	AndroidAudioPlugin *instance)
{
	delete instance;
}

void sample_plugin_prepare(AndroidAudioPlugin *plugin, AndroidAudioPluginBuffer* buffer)
{
}
void sample_plugin_activate(AndroidAudioPlugin *plugin) {}
void sample_plugin_process(AndroidAudioPlugin *plugin,
	AndroidAudioPluginBuffer* buffer,
	long timeoutInNanoseconds)
{
	/* do something */
}
void sample_plugin_deactivate(AndroidAudioPlugin *plugin) {}

AndroidAudioPluginState state;
const AndroidAudioPluginState* sample_plugin_get_state(AndroidAudioPlugin *plugin)
{
	return &state; /* fill it */
}

void sample_plugin_set_state(AndroidAudioPlugin *plugin, AndroidAudioPluginState *input)
{
	/* apply argument input */
}

AndroidAudioPlugin* sample_plugin_new(
	AndroidAudioPluginFactory *pluginFactory,
	const char* pluginUniqueId,
	int sampleRate,
	const AndroidAudioPluginExtension * const *extensions)
{
	return new AndroidAudioPlugin {
		NULL,
		sample_plugin_prepare,
		sample_plugin_activate,
		sample_plugin_process,
		sample_plugin_deactivate,
		sample_plugin_get_state,
		sample_plugin_set_state
		};
}

} // namespace dogfoodingaap

AndroidAudioPluginFactory* GetAndroidAudioPluginFactory ()
{
	return new AndroidAudioPluginFactory { dogfoodingaap::sample_plugin_new, dogfoodingaap::sample_plugin_delete };
}


void dogfooding_api()
{
	aap::PluginHost manager(NULL);
	auto paths = manager.instantiatePlugin(NULL);
	GetAndroidAudioPluginFactory();
}



