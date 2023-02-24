
#include <aap/android-audio-plugin.h>
#include <aap/ext/plugin-info.h>
#include <aap/ext/state.h>
#include <aap/ext/midi.h>
#include <aap/ext/parameters.h>
#include <cassert>
#include <cstring>
#include "cmidi2.h"

extern "C" {

#define PLUGIN_URI "urn:org.androidaudioplugin/samples/aapbarebonepluginsample/TestFilter"

#define PARAM_ID_VOLUME_L 0
#define PARAM_ID_VOLUME_R 1
#define PARAM_ID_DELAY_L 2
#define PARAM_ID_DELAY_R 3

typedef struct SamplePluginSpecific {
    AndroidAudioPluginHost host;
    float modL{0.5f};
    float modR{0.5f};
    float modL_pn[128];
    float modR_pn[128];
    uint32_t delayL{0};
    uint32_t delayR{0};
    int32_t midiInPort{-1};
    int32_t midiOutPort{-1};
    int32_t audioInPortL{-1};
    int32_t audioInPortR{-1};
    int32_t audioOutPortL{-1};
    int32_t audioOutPortR{-1};

    SamplePluginSpecific(AndroidAudioPluginHost *host) {
        this->host = *host;
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

void sample_plugin_prepare(AndroidAudioPlugin *plugin, aap_buffer_t *buffer) {
    auto ctx = (SamplePluginSpecific*) plugin->plugin_specific;
    auto ext = (aap_host_plugin_info_extension_t*) ctx->host.get_extension_data(&ctx->host, AAP_PLUGIN_INFO_EXTENSION_URI);
    assert(ext);
    auto pluginInfo = ext->get(&ctx->host, PLUGIN_URI);
    auto numPorts = pluginInfo.get_port_count(&pluginInfo);
    assert(buffer->num_ports(*buffer) >= numPorts);
    for (int32_t i = 0, n = numPorts; i < n; i++) {
        auto port = pluginInfo.get_port(&pluginInfo, i);
        switch (port.content_type(&port)) {
            case AAP_CONTENT_TYPE_MIDI2:
                switch (port.direction(&port)) {
                    case AAP_PORT_DIRECTION_INPUT:
                        ctx->midiInPort = i;
                        break;
                    case AAP_PORT_DIRECTION_OUTPUT:
                        ctx->midiOutPort = i;
                        break;
                }
                break;
            case AAP_CONTENT_TYPE_AUDIO:
                switch (port.direction(&port)) {
                    case AAP_PORT_DIRECTION_INPUT:
                        if (ctx->audioInPortL < 0)
                            ctx->audioInPortL = i;
                        else if (ctx->audioInPortR < 0)
                            ctx->audioInPortR = i;
                        break;
                    case AAP_PORT_DIRECTION_OUTPUT:
                        if (ctx->audioOutPortL < 0)
                            ctx->audioOutPortL = i;
                        else if (ctx->audioOutPortR < 0)
                            ctx->audioOutPortR = i;
                        break;
                }
                break;
            default:
                break;
        }
    }
}

void sample_plugin_activate(AndroidAudioPlugin *plugin) {}

bool readMidi2Parameter(uint8_t *group, uint8_t* channel, uint8_t* key, uint8_t* extra, uint16_t *index, float *value, cmidi2_ump* ump) {
    if (cmidi2_ump_get_message_type(ump) != CMIDI2_MESSAGE_TYPE_SYSEX8_MDS)
        return false;
    auto raw = (uint32_t*) ump;
    return aapReadMidi2ParameterSysex8(group, channel, key, extra, index, value, *raw, *(raw + 1), *(raw + 2), *(raw + 3));
}

void sample_plugin_process(AndroidAudioPlugin *plugin,
                           aap_buffer_t *buffer,
                           long timeoutInNanoseconds) {
    // apply super-simple delay processing with super-simple volume adjustment per channel.

    auto ctx = (SamplePluginSpecific*) plugin->plugin_specific;

    int size = buffer->num_frames(*buffer) * sizeof(float);

    auto fIL = (float *) buffer->get_buffer(*buffer, ctx->audioInPortL);
    auto fIR = (float *) buffer->get_buffer(*buffer, ctx->audioInPortR);
    auto fOL = (float *) buffer->get_buffer(*buffer, ctx->audioOutPortL);
    auto fOR = (float *) buffer->get_buffer(*buffer, ctx->audioOutPortR);

    // update parameters via MIDI2 messages
    auto midiSeq = (AAPMidiBufferHeader*) buffer->get_buffer(*buffer, ctx->midiInPort);
    auto midiSeqData = midiSeq + 1;
    if (midiSeq->length > 0) {
        CMIDI2_UMP_SEQUENCE_FOREACH(midiSeqData, midiSeq->length, iter) {
            auto ump = (cmidi2_ump*) iter;
            uint8_t paramGroup, paramChannel, paramKey{0}, paramExtra{0};
            uint16_t paramIndex;
            float paramValue;
            uint32_t rawIntValue;
            bool relative;
            switch (cmidi2_ump_get_message_type(ump)) {
                case CMIDI2_MESSAGE_TYPE_UTILITY:
                    if (cmidi2_ump_get_status_code(ump) == CMIDI2_JR_TIMESTAMP) {
                        // FIXME: ideally there should be timestamp-based sample accuracy adjustment here
                        //  and the audio processing code later should take care of it, but we're lazy here...
                    }
                    continue;
                case CMIDI2_MESSAGE_TYPE_SYSEX8_MDS: {
                    if (!readMidi2Parameter(&paramGroup, &paramChannel, &paramKey, &paramExtra,
                                            &paramIndex, &paramValue, ump))
                        continue;
                    rawIntValue = *(uint32_t *) &paramValue;
                } break;
                case CMIDI2_MESSAGE_TYPE_MIDI_2_CHANNEL: {
                    switch (cmidi2_ump_get_status_code(ump)) {
                        // enable this if it supports per-note parameters.
                        case CMIDI2_STATUS_PER_NOTE_ACC:
                            paramKey = cmidi2_ump_get_midi2_pnacc_note(ump); // FIXME: implement it maybe?
                            break;
                        case CMIDI2_STATUS_RELATIVE_NRPN:
                            relative = true; // FIXME: implement it maybe?
                            break;
                        case CMIDI2_STATUS_NRPN:
                            break;
                        default:
                            continue;
                    }
                    paramGroup = cmidi2_ump_get_group(ump);
                    paramChannel = cmidi2_ump_get_channel(ump);
process_acc:
                    paramIndex = cmidi2_ump_get_midi2_nrpn_msb(ump) * 0x80 + cmidi2_ump_get_midi2_nrpn_lsb(ump);
                    rawIntValue = cmidi2_ump_get_midi2_nrpn_data(ump);
                    paramValue = *(float*) &rawIntValue;
                } break;
            }
            float valueIn0To2048;
            float mod;
            uint32_t intValue;
            switch (paramIndex) {
                case PARAM_ID_VOLUME_L:
                    intValue = rawIntValue + (relative ? *(uint32_t*) (&ctx->modL) : 0);
                    mod = *(float*) (&intValue);
                    if (paramKey != 0)
                        ctx->modL_pn[paramKey] = mod;
                    else
                        ctx->modL = mod;
                    break;
                case PARAM_ID_VOLUME_R:
                    intValue = rawIntValue + (relative ? *(uint32_t*) (&ctx->modR) : 0);
                    mod = *(float*) (&intValue);
                    if (paramKey != 0)
                        ctx->modR_pn[paramKey] = mod;
                    else
                        ctx->modR = mod;
                    break;
                case PARAM_ID_DELAY_L:
                    if (paramKey != 0)
                        break; // FIXME: implement or log it?
                    valueIn0To2048 = *((float*) &rawIntValue);
                    ctx->delayL = (uint32_t) valueIn0To2048 + (relative ? ctx->delayL : 0);
                    break;
                case PARAM_ID_DELAY_R:
                    if (paramKey != 0)
                        break; // FIXME: implement or log it?
                    valueIn0To2048 = *((float*) &rawIntValue);
                    ctx->delayR = (uint32_t) valueIn0To2048 + (relative ? ctx->delayR : 0);
                    break;
                default:
                    continue; // invalid parameter index FIXME: log it
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
            new SamplePluginSpecific(host),
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
