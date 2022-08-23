
#include <aap/android-audio-plugin.h>
#include <aap/ext/state.h>
#include <aap/ext/aap-midi2.h>
#include <cassert>
#include <cstring>
#include "../../../../../external/cmidi2/cmidi2.h"

extern "C" {

#define MIDI_PORT_IN 0
#define MIDI_PORT_OUT 1
#define AUDIO_PORT_IN_L 2
#define AUDIO_PORT_IN_R 3
#define AUDIO_PORT_OUT_L 4
#define AUDIO_PORT_OUT_R 5
#define CONTROL_PORT_VOLUME_L 6
#define CONTROL_PORT_VOLUME_R 7
#define CONTROL_PORT_DELAY_L 8
#define CONTROL_PORT_DELAY_R 9

#define PARAM_ID_VOLUME_L 0
#define PARAM_ID_VOLUME_R 1
#define PARAM_ID_DELAY_L 2
#define PARAM_ID_DELAY_R 3

typedef struct SamplePluginSpecific {
    /* any kind of extension information, which will be passed as void* */
    float modL{0.5f};
    float modR{0.5f};
    float modL_pn[128];
    float modR_pn[128];
    uint32_t delayL{0};
    uint32_t delayR{0};

    SamplePluginSpecific() {
        for (size_t i = 0; i < 128; i++)
            modL_pn[i] = modR_pn[i] = 0.5f;
    }
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
    // apply super-simple delay processing with super-simple volume adjustment per channel.

    auto ctx = (SamplePluginSpecific*) plugin->plugin_specific;

    int size = buffer->num_frames * sizeof(float);

    /* This is the old way to support "parameters" via ports.
    ctx->modL = ((float *) buffer->buffers[CONTROL_PORT_VOLUME_L])[0];
    ctx->modR = ((float *) buffer->buffers[CONTROL_PORT_VOLUME_R])[0];
    ctx->delayL = (uint32_t) ((float *) buffer->buffers[CONTROL_PORT_DELAY_L])[0];
    ctx->delayR = (uint32_t) ((float *) buffer->buffers[CONTROL_PORT_DELAY_R])[0];
     */

    auto fIL = (float *) buffer->buffers[AUDIO_PORT_IN_L];
    auto fIR = (float *) buffer->buffers[AUDIO_PORT_IN_R];
    auto fOL = (float *) buffer->buffers[AUDIO_PORT_OUT_L];
    auto fOR = (float *) buffer->buffers[AUDIO_PORT_OUT_R];

    // update parameters via MIDI2 messages
    auto midiSeq = (AAPMidiBufferHeader*) buffer->buffers[MIDI_PORT_IN];
    auto midiSeqData = midiSeq + 1;
    if (midiSeq->length > 0) {
        CMIDI2_UMP_SEQUENCE_FOREACH(midiSeqData, midiSeq->length, iter) {
            auto ump = (cmidi2_ump*) iter;
            switch (cmidi2_ump_get_message_type(ump)) {
                case CMIDI2_MESSAGE_TYPE_UTILITY:
                    if (cmidi2_ump_get_status_code(ump) == CMIDI2_JR_TIMESTAMP) {
                        // FIXME: ideally there should be timestamp-based sample accuracy adjustment here
                        //  and the audio processing code later should take care of it, but we're lazy here...
                    }
                    break;
                case CMIDI2_MESSAGE_TYPE_MIDI_2_CHANNEL: {
                    bool relative = false;
                    bool perNote = false;
                    switch (cmidi2_ump_get_status_code(ump)) {
                        case CMIDI2_STATUS_PER_NOTE_ACC:
                            perNote = true;
                            break;
                        case CMIDI2_STATUS_RELATIVE_NRPN:
                            relative = true;
                            break;
                        case CMIDI2_STATUS_NRPN:
                            break;
                        default:
                            continue;
                    }
                    auto target = cmidi2_ump_get_midi2_nrpn_msb(ump) * 0x100 +
                                  cmidi2_ump_get_midi2_nrpn_lsb(ump);
                    uint32_t rawIntValue;
                    float valueIn0To2048;
                    float mod;
                    switch (target) {
                        case PARAM_ID_VOLUME_L:
                            mod = (float) ((cmidi2_ump_get_midi2_nrpn_data(ump) +
                                             (relative ? (uint32_t) (ctx->modL * UINT32_MAX) : 0)) /
                                            (double) UINT32_MAX);
                            if (perNote)
                                ctx->modL_pn[cmidi2_ump_get_midi2_pnacc_note(ump)] = mod;
                            else
                                ctx->modL = mod;
                            break;
                        case PARAM_ID_VOLUME_R:
                            mod = (float) ((cmidi2_ump_get_midi2_nrpn_data(ump) +
                                             (relative ? (uint32_t) (ctx->modR * UINT32_MAX) : 0)) /
                                            (double) UINT32_MAX);
                            if (perNote)
                                ctx->modR_pn[cmidi2_ump_get_midi2_pnacc_note(ump)] = mod;
                            else
                                ctx->modR = mod;
                            break;
                        case PARAM_ID_DELAY_L:
                            if (perNote)
                                break; // FIXME: implement or log it?
                            rawIntValue = (cmidi2_ump_get_midi2_nrpn_data(ump));
                            valueIn0To2048 = *((float*) (void*) &rawIntValue);
                            ctx->delayL = relative ? ctx->delayL + (uint32_t) valueIn0To2048 : (uint32_t) valueIn0To2048;
                            break;
                        case PARAM_ID_DELAY_R:
                            if (perNote)
                                break; // FIXME: implement or log it?
                            rawIntValue = (cmidi2_ump_get_midi2_nrpn_data(ump));
                            valueIn0To2048 = *((float*) (void*) &rawIntValue);
                            ctx->delayR = relative ? ctx->delayR + (uint32_t) valueIn0To2048 : (uint32_t) valueIn0To2048;
                            break;
                        default:
                            continue; // invalid parameter index FIXME: log it
                    }
                    break;
                }
            }
        }
    }

    for (int i = 0; i < size / sizeof(float); i++) {
        if (i >= ctx->delayL)
            fOL[i] = (float) (fIL[i - ctx->delayL] * ctx->modL);
        if (i >= ctx->delayR)
            fOR[i] = (float) (fIR[i - ctx->delayR] * ctx->modR);
    }

    /* FIXME: This is for testing minBufferSize, but now it's gone because we don't use port for it.
     *  Maybe we need some other way to test it...
    // buffers[10]-buffers[13] are dummy parameters, but try accessing buffers[10], which has
    // pp:minimumSize = 8192, to verify that buffers[8][8191] is touchable!
    int x = ((uint8_t*) buffer->buffers[10])[8191];
    if (x > 256)
        return; // NOOP
    */
}

void sample_plugin_deactivate(AndroidAudioPlugin *plugin) {}

size_t sample_plugin_get_state_size(AndroidAudioPluginExtensionTarget target) {
    return 0;
}

void sample_plugin_get_state(AndroidAudioPluginExtensionTarget target, aap_state_t* state) {
    // FIXME: implement
}

void sample_plugin_set_state(AndroidAudioPluginExtensionTarget target, aap_state_t* input) {
    // FIXME: implement
}

aap_state_extension_t state_extension{sample_plugin_get_state_size,
                                      sample_plugin_get_state,
                                      sample_plugin_set_state};

void* sample_plugin_get_extension(AndroidAudioPlugin *, const char* uri) {
    if (!strcmp(uri, AAP_STATE_EXTENSION_URI))
        return &state_extension;
    return nullptr;
}

AndroidAudioPlugin *sample_plugin_new(
        AndroidAudioPluginFactory *pluginFactory,
        const char *pluginUniqueId,
        int sampleRate,
        AndroidAudioPluginHost *host) {
    return new AndroidAudioPlugin{
            new SamplePluginSpecific(),
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
