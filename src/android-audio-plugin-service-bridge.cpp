#include "../include/android-audio-plugin.h"


void aap_bridge_plugin_delete(
	AndroidAudioPluginFactory *pluginFactory,
	AndroidAudioPlugin *instance)
{
	delete instance;
}

void aap_bridge_plugin_prepare(AndroidAudioPlugin *plugin, AndroidAudioPluginBuffer* buffer)
{
}

void aap_bridge_plugin_activate(AndroidAudioPlugin *plugin)
{
}

void aap_bridge_plugin_process(AndroidAudioPlugin *plugin,
	AndroidAudioPluginBuffer* buffer,
	long timeoutInNanoseconds)
{
	/* do something */
}

void aap_bridge_plugin_deactivate(AndroidAudioPlugin *plugin)
{
}

AndroidAudioPluginState state;
const AndroidAudioPluginState* aap_bridge_plugin_get_state(AndroidAudioPlugin *plugin)
{
	return &state; /* fill it */
}

void aap_bridge_plugin_set_state(AndroidAudioPlugin *plugin, AndroidAudioPluginState *input)
{
	/* apply argument input */
}

AndroidAudioPlugin* aap_bridge_plugin_new(
	AndroidAudioPluginFactory *pluginFactory,
	const char* pluginUniqueId,
	int aap_bridgeRate,
	const AndroidAudioPluginExtension * const *extensions)
{
	return new AndroidAudioPlugin {
		NULL,
		aap_bridge_plugin_prepare,
		aap_bridge_plugin_activate,
		aap_bridge_plugin_process,
		aap_bridge_plugin_deactivate,
		aap_bridge_plugin_get_state,
		aap_bridge_plugin_set_state
		};
}

AndroidAudioPluginFactory* GetAndroidAudioPluginFactoryServiceBridge ()
{
	return new AndroidAudioPluginFactory { aap_bridge_plugin_new, aap_bridge_plugin_delete };
}
