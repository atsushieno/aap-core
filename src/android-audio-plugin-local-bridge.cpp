#include <sys/mman.h>
#include <cstdlib>
#include <android/sharedmem.h>
#include <android/log.h>
#include "../include/android-audio-plugin.h"
#include "org/androidaudiopluginframework/AudioPluginInterface.h"
#include "../include/android-audio-plugin-host.hpp"

class AAPLocalContext {
public:
	AndroidAudioPluginBuffer *previous_buffer;
	AndroidAudioPluginState state;
	aap::PluginHost *host;
	aap::PluginInstance *instance;
	int sample_rate;

	const char* getPluginId()
    {
        return instance->getPluginDescriptor()->getPluginID();
    }
};

void aap_local_bridge_plugin_prepare(AndroidAudioPlugin *plugin, AndroidAudioPluginBuffer* buffer)
{
    __android_log_write(ANDROID_LOG_DEBUG, "AAP_LOCAL_BRIDGE", "aap_local_bridge_plugin_prepare");

    auto ctx = (AAPLocalContext*) plugin->plugin_specific;
    // FIXME: replace 2 with the actual audio channel count.
    ctx->instance->prepare(ctx->sample_rate, buffer->num_frames * 2, buffer);
}

void aap_local_bridge_plugin_activate(AndroidAudioPlugin *plugin)
{
    auto ctx = (AAPLocalContext*) plugin->plugin_specific;
    ctx->instance->activate();
}

void aap_local_bridge_plugin_process(AndroidAudioPlugin *plugin,
	AndroidAudioPluginBuffer* buffer,
	long timeoutInNanoseconds)
{
    auto ctx = (AAPLocalContext*) plugin->plugin_specific;
    ctx->instance->process(buffer, timeoutInNanoseconds);
}

void aap_local_bridge_plugin_deactivate(AndroidAudioPlugin *plugin)
{
    auto ctx = (AAPLocalContext*) plugin->plugin_specific;
    ctx->instance->deactivate();
}

const AndroidAudioPluginState* aap_local_bridge_plugin_get_state(AndroidAudioPlugin *plugin)
{
    auto ctx = (AAPLocalContext*) plugin->plugin_specific;
    ctx->state.data_size = ctx->instance->getStateSize();
    ctx->state.raw_data = ctx->instance->getState();
    return &ctx->state;
}

void aap_local_bridge_plugin_set_state(AndroidAudioPlugin *plugin, AndroidAudioPluginState *input)
{
    auto ctx = (AAPLocalContext*) plugin->plugin_specific;
    ctx->instance->setState(input->raw_data, 0, input->data_size);
}

extern aap::PluginInformation **local_plugin_infos;

AndroidAudioPlugin* aap_local_bridge_plugin_new(
	AndroidAudioPluginFactory *pluginFactory,	// unused
	const char* pluginUniqueId,
	int sampleRate,
	const AndroidAudioPluginExtension * const *extensions	// unused
	)
{
    __android_log_write(ANDROID_LOG_DEBUG, "AAP_LOCAL_BRIDGE", "aap_local_bridge_plugin_new");

    auto ctx = new AAPLocalContext();
    ctx->host = new aap::PluginHost(local_plugin_infos);
    ctx->instance = ctx->host->instantiatePlugin(pluginUniqueId);
    ctx->sample_rate = sampleRate;
	return new AndroidAudioPlugin {
		ctx,
        aap_local_bridge_plugin_prepare,
        aap_local_bridge_plugin_activate,
        aap_local_bridge_plugin_process,
        aap_local_bridge_plugin_deactivate,
        aap_local_bridge_plugin_get_state,
        aap_local_bridge_plugin_set_state
		};
}

void aap_local_bridge_plugin_delete(
		AndroidAudioPluginFactory *pluginFactory,	// unused
		AndroidAudioPlugin *instance)
{
	auto ctx = (AAPLocalContext*) instance->plugin_specific;
	ctx->instance->dispose();
	delete ctx->instance;
	delete ctx->host;
	delete ctx;
	delete instance;
}

extern "C" {

AndroidAudioPluginFactory *GetAndroidAudioPluginFactoryLocalBridge() {
    __android_log_write(ANDROID_LOG_DEBUG, "AAP_LOCAL_BRIDGE", "GetAndroidAudioPluginFactoryLocalBridge");

    return new AndroidAudioPluginFactory{aap_local_bridge_plugin_new, aap_local_bridge_plugin_delete};
}
}
