
#include <aap/android-audio-plugin.h>
#include <aap/unstable/state.h>
#include <cstring>

extern "C" {

typedef struct {
    /* any kind of extension information, which will be passed as void* */
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

void sample_plugin_process(AndroidAudioPlugin *plugin,
                           AndroidAudioPluginBuffer *buffer,
                           long timeoutInNanoseconds) {

    // apply super-simple delay processing
    int size = buffer->num_frames * sizeof(float);

    auto modL = ((float *) buffer->buffers[4])[0];
    auto modR = ((float *) buffer->buffers[5])[0];
    auto delayL = (int) ((float *) buffer->buffers[6])[0];
    auto delayR = (int) ((float *) buffer->buffers[7])[0];
    auto fIL = (float *) buffer->buffers[0];
    auto fIR = (float *) buffer->buffers[1];
    auto fOL = (float *) buffer->buffers[2];
    auto fOR = (float *) buffer->buffers[3];
    for (int i = 0; i < size / sizeof(float); i++) {
        if (i >= delayL)
            fOL[i] = (float) (fIL[i - delayL] * modL);
        if (i >= delayR)
            fOR[i] = (float) (fIR[i - delayR] * modR);
    }

    /* do anything. In this example, inputs (0,1) are directly copied to outputs (2,3) */
    /*
    memcpy(buffer->buffers[2], buffer->buffers[0], size);
    memcpy(buffer->buffers[3], buffer->buffers[1], size);
    */

    // buffers[8]-buffers[11] are dummy parameters, but try accessing buffers[8], which has
    // pp:minimumSize = 8192, to verify that buffers[8][8191] is touchable!
    int x = ((uint8_t*) buffer->buffers[8])[8191];
    if (x > 256)
        return; // NOOP
}

void sample_plugin_deactivate(AndroidAudioPlugin *plugin) {}

void sample_plugin_get_state(AndroidAudioPlugin *plugin, aap_state_t* state) {
    // FIXME: implement
}

void sample_plugin_set_state(AndroidAudioPlugin *plugin, aap_state_t* input) {
    // FIXME: implement
}

void* sample_plugin_get_extension(AndroidAudioPlugin *plugin, const char* uri) {
    return nullptr;
}

AndroidAudioPlugin *sample_plugin_new(
        AndroidAudioPluginFactory *pluginFactory,
        const char *pluginUniqueId,
        int sampleRate,
        AndroidAudioPluginHost *host) {
    return new AndroidAudioPlugin{
            new SamplePluginSpecific{},
            sample_plugin_prepare,
            sample_plugin_activate,
            sample_plugin_process,
            sample_plugin_deactivate,
            sample_plugin_get_extension
    };
}

AndroidAudioPluginFactory factory{sample_plugin_new, sample_plugin_delete};

AndroidAudioPluginFactory *GetAndroidAudioPluginFactory() {
    return &factory;
}

}
