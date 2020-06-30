
#include <aap/android-audio-plugin.h>
#include <cstring>

extern "C" {

void sample_plugin_delete(
		AndroidAudioPluginFactory *pluginFactory,
		AndroidAudioPlugin *instance) {
	delete instance;
}

void sample_plugin_prepare(AndroidAudioPlugin *plugin, AndroidAudioPluginBuffer *buffer) {
	/* do something */
}

void sample_plugin_activate(AndroidAudioPlugin *plugin) {}

void sample_plugin_process(AndroidAudioPlugin *plugin,
						   AndroidAudioPluginBuffer *buffer,
						   long timeoutInNanoseconds) {
	/* do anything. In this example, inputs (0,1) are directly copied to outputs (2,3) */
	int size = buffer->num_frames * sizeof(float);
	memcpy(buffer->buffers[2], buffer->buffers[0], size);
	memcpy(buffer->buffers[3], buffer->buffers[1], size);
	/*
	for (int i = 0; i < buffer->num_buffers; i++) {
		for (int x = 0; x < size; x++)
			((char*) buffer->buffers[i])[x] = (char) ((x % 8) + (i * 8));
	}*/
}

void sample_plugin_deactivate(AndroidAudioPlugin *plugin) {}

void sample_plugin_get_state(AndroidAudioPlugin *plugin, AndroidAudioPluginState* state) {
	// FIXME: implement
}

void sample_plugin_set_state(AndroidAudioPlugin *plugin, AndroidAudioPluginState *input) {
	// FIXME: implement
}

typedef struct {
	/* any kind of extension information, which will be passed as void* */
} SamplePluginSpecific;

AndroidAudioPlugin *sample_plugin_new(
		AndroidAudioPluginFactory *pluginFactory,
		const char *pluginUniqueId,
		int sampleRate,
		AndroidAudioPluginExtension** extensions) {
	return new AndroidAudioPlugin{
			new SamplePluginSpecific{},
			sample_plugin_prepare,
			sample_plugin_activate,
			sample_plugin_process,
			sample_plugin_deactivate,
			sample_plugin_get_state,
			sample_plugin_set_state
	};
}

AndroidAudioPluginFactory *GetAndroidAudioPluginFactory() {
	return new AndroidAudioPluginFactory{sample_plugin_new, sample_plugin_delete};
}

}
