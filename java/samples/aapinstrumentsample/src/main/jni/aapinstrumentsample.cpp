
#include <aap/android-audio-plugin.h>
#include <cstring>
#include <math.h>

extern "C" {

typedef struct {
    /* any kind of extension information, which will be passed as void* */
    int sample_rate;
} SamplePluginSpecific;

void sample_plugin_delete(
        AndroidAudioPluginFactory *pluginFactory,
        AndroidAudioPlugin *instance) {
    delete (SamplePluginSpecific*) instance->plugin_specific;
    delete instance;
}

void sample_plugin_prepare(AndroidAudioPlugin *plugin, AndroidAudioPluginBuffer *buffer) {
    /* do something */
}

void sample_plugin_activate(AndroidAudioPlugin *plugin) {}

int32_t start_offset{0};
int32_t current_freq = 440;

void sample_plugin_process(AndroidAudioPlugin *plugin,
                           AndroidAudioPluginBuffer *buffer,
                           long timeoutInNanoseconds) {

    auto context = (SamplePluginSpecific*) plugin->plugin_specific;

    auto sampleRate = context->sample_rate;

    auto outL = (float*) buffer->buffers[2];
    auto outR = (float*) buffer->buffers[3];
    int32_t start = 0;
    int32_t end = buffer->num_frames;
    uint8_t note = 60;
    for (int i = start; i < end; i++) {
        auto sampleFreqL = (pow(2.0, ((note - 57) / 12.0))) * current_freq;
        auto sampleFreqR = (pow(2.0, ((note - 24 - 57) / 12.0))) * current_freq;
        outL[i] = (float) sin((i + start_offset) / (sampleFreqL / (M_PI * 2)));
        outR[i] = (float) sin((i + start_offset) / (sampleFreqR / (M_PI * 2)));
    }
    start_offset = (start_offset + end - start) % sampleRate;
}

void sample_plugin_deactivate(AndroidAudioPlugin *plugin) {}

void sample_plugin_get_state(AndroidAudioPlugin *plugin, AndroidAudioPluginState *state) {
    // FIXME: implement
}

void sample_plugin_set_state(AndroidAudioPlugin *plugin, AndroidAudioPluginState *input) {
    // FIXME: implement
}

AndroidAudioPlugin *sample_plugin_new(
        AndroidAudioPluginFactory *pluginFactory,
        const char *pluginUniqueId,
        int sampleRate,
        AndroidAudioPluginExtension **extensions) {
    return new AndroidAudioPlugin{
            new SamplePluginSpecific{sampleRate},
            sample_plugin_prepare,
            sample_plugin_activate,
            sample_plugin_process,
            sample_plugin_deactivate,
            sample_plugin_get_state,
            sample_plugin_set_state
    };
}

AndroidAudioPluginFactory factory{sample_plugin_new, sample_plugin_delete};

AndroidAudioPluginFactory *GetAndroidAudioPluginFactory() {
    return &factory;
}

}
