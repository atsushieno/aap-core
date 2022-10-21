
#include <aap/android-audio-plugin.h>
#include <cstring>
#include <math.h>
#include <aap/unstable/logging.h>
#include <aap/ext/presets.h>
#include <aap/ext/state.h>

extern "C" {

#include "ayumi.h"
#include "cmidi2.h"
#include "aap/ext/aap-midi2.h"

#define AYUMI_AAP_MIDI2_IN_PORT 0
#define AYUMI_AAP_MIDI2_OUT_PORT 1
#define AYUMI_AAP_AUDIO_OUT_LEFT 2
#define AYUMI_AAP_AUDIO_OUT_RIGHT 3
#define AYUMI_AAP_PARAM_ENVELOPE 0x0F
#define AYUMI_AAP_MIDI_CC_ENVELOPE_H 0x10
#define AYUMI_AAP_MIDI_CC_ENVELOPE_M 0x11
#define AYUMI_AAP_MIDI_CC_ENVELOPE_L 0x12
#define AYUMI_AAP_MIDI_CC_ENVELOPE_SHAPE 0x13

typedef struct {
    struct ayumi* impl;
    int mixer[3];
    int32_t envelope;
    int32_t pitchbend;
    double sample_rate;
    bool active;
    bool note_on_state[3];
    int32_t midi_protocol;
    int32_t preset_index{-1};
} AyumiHandle;


void sample_plugin_delete(
        AndroidAudioPluginFactory *pluginFactory,
        AndroidAudioPlugin *instance) {
    auto context = (AyumiHandle*) instance->plugin_specific;
    delete context->impl;
    delete context;
    delete instance;
}

void sample_plugin_prepare(AndroidAudioPlugin *plugin, AndroidAudioPluginBuffer *buffer) {
    /* do something */
}

void sample_plugin_activate(AndroidAudioPlugin *plugin) {
    auto context = (AyumiHandle*) plugin->plugin_specific;
    context->active = true;
}


double key_to_freq(double key) {
    // We use equal temperament
    // https://pages.mtu.edu/~suits/NoteFreqCalcs.html
    double ret = 220.0 * pow(1.059463, key - 45.0);
    return ret;
}

void ayumi_aap_process_midi_event(AyumiHandle *a, uint8_t *midi1Event) {
    int noise, tone_switch, noise_switch, env_switch;
    uint8_t * msg = midi1Event;
    int channel = msg[0] & 0xF;
    if (channel > 2)
        return;
    int mixer;
    switch (msg[0] & 0xF0) {
        case CMIDI2_STATUS_NOTE_OFF: note_off:
            if (!a->note_on_state[channel])
                break; // not at note on state
            ayumi_set_mixer(a->impl, channel, 1, 1, 0);
            a->note_on_state[channel] = false;
            break;
        case CMIDI2_STATUS_NOTE_ON:
            if (msg[2] == 0)
                goto note_off; // it is illegal though.
            if (a->note_on_state[channel])
                break; // busy
            mixer = a->mixer[channel];
            tone_switch = (mixer >> 5) & 1;
            noise_switch = (mixer >> 6) & 1;
            env_switch = (mixer >> 7) & 1;
            ayumi_set_mixer(a->impl, channel, tone_switch, noise_switch, env_switch);
            ayumi_set_tone(a->impl, channel, 2000000.0 / (16.0 * key_to_freq(msg[1])));
            a->note_on_state[channel] = true;
            break;
        case CMIDI2_STATUS_PROGRAM:
            noise = msg[1] & 0x1F;
            ayumi_set_noise(a->impl, noise);
            mixer = msg[1];
            tone_switch = (mixer >> 5) & 1;
            noise_switch = (mixer >> 6) & 1;
            // We cannot pass 8 bit message, so we remove env_switch here. Use BankMSB for it.
            env_switch = (a->mixer[channel] >> 7) & 1;
            a->mixer[channel] = msg[1];
            ayumi_set_mixer(a->impl, channel, tone_switch, noise_switch, env_switch);
            break;
        case CMIDI2_STATUS_CC:
            switch (msg[1]) {
                case CMIDI2_CC_BANK_SELECT:
                    mixer = msg[1];
                    tone_switch = mixer & 1;
                    noise_switch = (mixer >> 1) & 1;
                    env_switch = (mixer >> 2) & 1;
                    a->mixer[channel] = msg[1] << 5;
                    ayumi_set_mixer(a->impl, channel, tone_switch, noise_switch, env_switch);
                    break;
                case CMIDI2_CC_PAN:
                    ayumi_set_pan(a->impl, channel, msg[2] / 128.0, 0);
                    break;
                case CMIDI2_CC_VOLUME:
                    ayumi_set_volume(a->impl, channel, (msg[2] > 119 ? 119 : msg[2]) / 8); // FIXME: max is 14?? 15 doesn't work
                    break;
                case AYUMI_AAP_MIDI_CC_ENVELOPE_H:
                    a->envelope = (a->envelope & 0x3FFF) + (msg[2] << 14);
                    ayumi_set_envelope(a->impl, a->envelope);
                    break;
                case AYUMI_AAP_MIDI_CC_ENVELOPE_M:
                    a->envelope = (a->envelope & 0xC07F) + (msg[2] << 7);
                    ayumi_set_envelope(a->impl, a->envelope);
                    break;
                case AYUMI_AAP_MIDI_CC_ENVELOPE_L:
                    a->envelope = (a->envelope & 0xFF80) + msg[2];
                    ayumi_set_envelope(a->impl, a->envelope);
                    break;
                case AYUMI_AAP_MIDI_CC_ENVELOPE_SHAPE:
                    ayumi_set_envelope_shape(a->impl, msg[2] & 0xF);
                    break;
            }
            break;
        case CMIDI2_STATUS_PITCH_BEND:
            a->pitchbend = (msg[1] << 7) + msg[2];
            break;
        default:
            break;
    }
}

void sample_plugin_process(AndroidAudioPlugin *plugin,
                           AndroidAudioPluginBuffer *buffer,
                           long timeoutInNanoseconds) {

    auto context = (AyumiHandle*) plugin->plugin_specific;
    if (!context->active)
        return;

    volatile auto aapmb = (AAPMidiBufferHeader*) buffer->buffers[AYUMI_AAP_MIDI2_IN_PORT];
    int32_t lengthUnit = aapmb->time_options;

    uint32_t currentTicks = 0;

    auto outL = (float*) buffer->buffers[AYUMI_AAP_AUDIO_OUT_LEFT];
    auto outR = (float*) buffer->buffers[AYUMI_AAP_AUDIO_OUT_RIGHT];

    if (context->midi_protocol == AAP_PROTOCOL_MIDI2_0) {
        auto midi2ptr = ((uint32_t*) (void*) aapmb) + 8;
        CMIDI2_UMP_SEQUENCE_FOREACH(midi2ptr, aapmb->length, ev) {
            auto ump = (cmidi2_ump *) ev;
            if (cmidi2_ump_get_message_type(ump) == CMIDI2_MESSAGE_TYPE_UTILITY &&
                cmidi2_ump_get_status_code(ump) == CMIDI2_JR_TIMESTAMP) {
                uint32_t max = currentTicks + (uint32_t) (cmidi2_ump_get_jr_timestamp_timestamp(ump) / 31250.0 * context->sample_rate);
                max = max < buffer->num_frames ? max : buffer->num_frames;
                for (uint32_t i = currentTicks; i < max; i++) {
                    ayumi_process(context->impl);
                    ayumi_remove_dc(context->impl);
                    outL[i] = (float) context->impl->left;
                    outR[i] = (float) context->impl->right;
                }
                currentTicks = max;
                continue;
            } else if (cmidi2_ump_get_message_type(ump) == CMIDI2_MESSAGE_TYPE_MIDI_2_CHANNEL &&
                       cmidi2_ump_get_status_code(ump) == CMIDI2_STATUS_NRPN) {
                auto channel = cmidi2_ump_get_channel(ump);
                auto data7bit = (int32_t) (cmidi2_ump_get_midi2_nrpn_data(ump) >> 25) & 0x7F; // get 7-bit data
                switch ((cmidi2_ump_get_midi2_nrpn_msb(ump) << 7) + cmidi2_ump_get_midi2_nrpn_lsb(ump)) {
                    case CMIDI2_CC_BANK_SELECT: {
                        uint32_t value = cmidi2_ump_get_midi2_nrpn_data(ump);
                        auto mixer = (int32_t) *(float*) &value;
                        auto tone_switch = mixer & 1;
                        auto noise_switch = (mixer >> 1) & 1;
                        auto env_switch = (mixer >> 2) & 1;
                        context->mixer[channel] = mixer << 5;
                        ayumi_set_mixer(context->impl, channel, tone_switch, noise_switch,
                                        env_switch);
                        break;
                    }
                    case CMIDI2_CC_PAN: {
                        uint32_t value = cmidi2_ump_get_midi2_nrpn_data(ump);
                        float pan = *(float*) &value;
                        ayumi_set_pan(context->impl, channel, pan, 0);
                        break;
                    }
                    case CMIDI2_CC_VOLUME: {
                        uint32_t value = cmidi2_ump_get_midi2_nrpn_data(ump);
                        auto volume = (int32_t) *(float*) &value;
                        ayumi_set_volume(context->impl, channel, volume);
                        break;
                    }
                    case AYUMI_AAP_PARAM_ENVELOPE: {
                        uint32_t value = cmidi2_ump_get_midi2_nrpn_data(ump);
                        auto env = ((int32_t) *(float *) &value) & 0xFFFF;
                        context->envelope = env;
                        ayumi_set_envelope(context->impl, context->envelope);
                        break;
                    }
                    case AYUMI_AAP_MIDI_CC_ENVELOPE_SHAPE:
                        uint32_t value = cmidi2_ump_get_midi2_nrpn_data(ump);
                        auto shape = (int32_t) *(float*) &value;
                        ayumi_set_envelope_shape(context->impl, shape);
                        break;
                }
            } else {
                // FIXME: fully downconvert to MIDI1 and process it (sysex can be lengthier)
                uint8_t midi1Bytes[16];
                if (cmidi2_convert_single_ump_to_midi1(midi1Bytes, 16, ump) > 0)
                    ayumi_aap_process_midi_event(context, midi1Bytes);
            }
        }
    } else {
        auto midi1ptr = ((uint8_t*) (void*) aapmb) + 32;
        auto midi1end = midi1ptr + aapmb->length;
        while (midi1ptr < midi1end) {
            // get delta time
            uint32_t deltaTime = cmidi2_midi1_get_7bit_encoded_int(midi1ptr, midi1end - midi1ptr);
            midi1ptr += cmidi2_midi1_get_7bit_encoded_int_length(deltaTime);

            // Check if the message is Set New Protocol and promote Protocol.
            if (midi1end - midi1ptr >= 19 && *midi1ptr == 0xF0) {
                int32_t protocol = cmidi2_ci_try_parse_new_protocol(midi1ptr + 1, 19);
                if (protocol != 0) {
                    context->midi_protocol = protocol;
                    // At this state, we discard any remaining buffer as Set New Protocol should be
                    // sent only by itself.
                    // MIDI-CI specification explicitly tells that there should be some intervals (100msec).
                    aap::aprintf("MIDI-CI Set New Protocol received: %d", protocol);
                    break;
                }
            }

            // process audio until current time (max)
            uint32_t deltaTicks = lengthUnit < 0 ?
                    (uint32_t) ((context->sample_rate / -lengthUnit) * (deltaTime % 0x100)) + (uint32_t) (context->sample_rate * (deltaTime / 0x100 % 60)) :  // assuming values beyond minute in SMPTE don't matter.
                    (uint32_t) (1.0 * deltaTime / 240); // LAMESPEC: we should deprecate ticks specification.
            uint32_t max = currentTicks + deltaTicks;
            max = max < buffer->num_frames ? max : buffer->num_frames;
            for (uint32_t i = currentTicks; i < max; i++) {
                ayumi_process(context->impl);
                ayumi_remove_dc(context->impl);
                outL[i] = (float) context->impl->left;
                outR[i] = (float) context->impl->right;
            }
            currentTicks = max;

            // process next MIDI event
            ayumi_aap_process_midi_event(context, midi1ptr);
            // progress midi1ptr
            midi1ptr += cmidi2_midi1_get_message_size(midi1ptr, midi1end - midi1ptr);
        }
    }

    for (int i = currentTicks; i < buffer->num_frames; i++) {
        ayumi_process(context->impl);
        ayumi_remove_dc(context->impl);
        outL[i] = (float) context->impl->left;
        outR[i] = (float) context->impl->right;
    }
}

void sample_plugin_deactivate(AndroidAudioPlugin *plugin) {
    auto context = (AyumiHandle*) plugin->plugin_specific;
    context->active = false;
}

size_t sample_plugin_get_state_size(AndroidAudioPluginExtensionTarget target) {
    // FIXME: implement
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
                                          sample_plugin_set_state,
};

// Preset support
uint8_t preset_data[][3] {{10}, {20}, {30}};

aap_preset_t presets[3] {
    {0, "preset1", preset_data[0], sizeof(preset_data[0])},
    {1, "preset2", preset_data[1], sizeof(preset_data[1])},
    {2, "preset3", preset_data[2], sizeof(preset_data[2])}
};

int32_t sample_plugin_get_preset_count(AndroidAudioPluginExtensionTarget /*target*/) {
    return sizeof(presets) / sizeof(aap_preset_t);
}

int32_t sample_plugin_get_preset_data_size(AndroidAudioPluginExtensionTarget /*target*/, int32_t index) {
    return presets[index].data_size; // just for testing, no actual content.
}

void sample_plugin_get_preset(AndroidAudioPluginExtensionTarget /*target*/, int32_t index, bool skipContent, aap_preset_t* preset) {
    preset->index = index;
    strncpy(preset->name, presets[index].name, AAP_PRESETS_EXTENSION_MAX_NAME_LENGTH);
    preset->data_size = presets[index].data_size;
    if (!skipContent && preset->data_size > 0)
        memcpy(preset->data, presets[index].data, preset->data_size);
}

int32_t sample_plugin_get_preset_index(AndroidAudioPluginExtensionTarget target) {
    auto handle = (AyumiHandle*) target.plugin->plugin_specific;
    return handle->preset_index;
}

void sample_plugin_set_preset_index(AndroidAudioPluginExtensionTarget target, int32_t index) {
    auto handle = (AyumiHandle*) target.plugin->plugin_specific;
    handle->preset_index = index;
}

aap_presets_extension_t presets_extension{sample_plugin_get_preset_count,
                                          sample_plugin_get_preset_data_size,
                                          sample_plugin_get_preset,
                                          sample_plugin_get_preset_index,
                                          sample_plugin_set_preset_index};

void* sample_plugin_get_extension(AndroidAudioPlugin* plugin, const char *uri) {
    if (strcmp(uri, AAP_STATE_EXTENSION_URI) == 0)
        return &state_extension;
    if (strcmp(uri, AAP_PRESETS_EXTENSION_URI) == 0)
        return &presets_extension;
    return nullptr;
}

AndroidAudioPlugin *sample_plugin_new(
        AndroidAudioPluginFactory *pluginFactory,
        const char *pluginUniqueId,
        int sampleRate,
        AndroidAudioPluginHost *host) {

    auto handle = new AyumiHandle();
    handle->active = false;
    handle->impl = (struct ayumi*) calloc(sizeof(struct ayumi), 1);
    handle->sample_rate = sampleRate;

    /* clock_rate / (sample_rate * 8 * 8) must be < 1.0 */
    ayumi_configure(handle->impl, 1, 2000000, (int) sampleRate);
    ayumi_set_noise(handle->impl, 4); // pink noise by default
    for (int i = 0; i < 3; i++) {
        handle->mixer[i] = 1 << 6; // tone, without envelope
        ayumi_set_pan(handle->impl, i, 0.5, 0); // 0(L)...1(R)
        ayumi_set_mixer(handle->impl, i, 1, 1, 0); // should be quiet by default
        ayumi_set_envelope_shape(handle->impl, 14); // see http://fmpdoc.fmp.jp/%E3%82%A8%E3%83%B3%E3%83%99%E3%83%AD%E3%83%BC%E3%83%97%E3%83%8F%E3%83%BC%E3%83%89%E3%82%A6%E3%82%A7%E3%82%A2/
        ayumi_set_envelope(handle->impl, 0x40); // somewhat slow
        ayumi_set_volume(handle->impl, i, 14); // FIXME: max = 14?? 15 doesn't work
    }

    // see if the host supports MIDI CI extension data.
    // Note that it is querying host capability, not the plugin extension.
    auto data = (aap_midi2_extension_t*) host->get_extension_data(host, AAP_MIDI2_EXTENSION_URI);
    if (data)
        handle->midi_protocol = data->protocol == 2 ? AAP_PROTOCOL_MIDI2_0 : AAP_PROTOCOL_MIDI1_0;
    //handle->midi_protocol = 2; // this is for testing MIDI2 in port.

    return new AndroidAudioPlugin{
            handle,
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
