
#include <aap/android-audio-plugin.h>
#include <cstring>
#include <string>
#include <math.h>
#include <aap/unstable/logging.h>
#include <aap/ext/presets.h>
#include <aap/ext/state.h>
#include <aap/ext/midi.h>
#include <aap/ext/plugin-info.h>
#include <aap/ext/parameters.h>
#include <aap/ext/gui.h>
#include "cmidi2.h"

extern "C" {
#include "ayumi.h"
}

#define AAP_APP_LOG_TAG "AAPInstrumentSample"

#define AYUMI_AAP_PARAM_ENVELOPE 0x0F
// Plugin Parameter index is MIDI_CC, including below.
#define AYUMI_AAP_MIDI_CC_ENVELOPE_H 0x10
#define AYUMI_AAP_MIDI_CC_ENVELOPE_M 0x11
#define AYUMI_AAP_MIDI_CC_ENVELOPE_L 0x12
#define AYUMI_AAP_MIDI_CC_ENVELOPE_SHAPE 0x13

typedef struct AyumiHandle {
    struct ayumi* impl;
    int mixer[3];
    int32_t envelope;
    int32_t pitchbend;
    double sample_rate;
    bool active;
    bool note_on_state[3];
    int32_t midi_protocol;
    int32_t preset_index{-1};
    AndroidAudioPluginHost host;
    std::string plugin_id;
    int32_t midi2_in_port{-1};
    int32_t midi2_out_port{-1};
    int32_t audio_out_l_port{-1};
    int32_t audio_out_r_port{-1};
} AyumiHandle;


void sample_plugin_delete(
        AndroidAudioPluginFactory *pluginFactory,
        AndroidAudioPlugin *instance) {
    auto context = (AyumiHandle*) instance->plugin_specific;
    delete context->impl;
    delete context;
    delete instance;
}

void sample_plugin_prepare(AndroidAudioPlugin *plugin, aap_buffer_t *buffer) {
    /* do something */
    auto context = (AyumiHandle*) plugin->plugin_specific;
    auto host = context->host;

    auto pluginInfoExt = (aap_host_plugin_info_extension_t*) host.get_extension(&host, AAP_PLUGIN_INFO_EXTENSION_URI);
    if (pluginInfoExt != nullptr) {
        auto info = pluginInfoExt->get(pluginInfoExt, &host, context->plugin_id.c_str());
        aap::a_log_f(AAP_LOG_LEVEL_INFO, AAP_APP_LOG_TAG, "plugin-info test: displayName: %s", info.display_name(&info));
        for (uint32_t i = 0; i < info.get_port_count(&info); i++) {
            auto port = info.get_port(&info, i);
            if (port.content_type(&port) == AAP_CONTENT_TYPE_MIDI2) {
                if (port.direction(&port) == AAP_PORT_DIRECTION_INPUT)
                    context->midi2_in_port = i;
                else
                    context->midi2_out_port = i;
            } else if (port.content_type(&port) == AAP_CONTENT_TYPE_AUDIO) {
                if (port.direction(&port) != AAP_PORT_DIRECTION_OUTPUT)
                    continue;
                if (context->audio_out_l_port < 0)
                    context->audio_out_l_port = i;
                else if (context->audio_out_r_port < 0)
                    context->audio_out_r_port = i;
            }
            aap::a_log_f(AAP_LOG_LEVEL_INFO, AAP_APP_LOG_TAG, "  plugin-info test: port %d: %s %s %s",
                         port.index(&port),
                         port.content_type(&port) == AAP_CONTENT_TYPE_AUDIO ? "AUDIO" : port.content_type(&port) == AAP_CONTENT_TYPE_MIDI2 ? "MIDI2" : "Other",
                         port.direction(&port) == AAP_PORT_DIRECTION_INPUT ? "IN" : "OUT",
                         port.name(&port));
        }
    }
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

bool readMidi2Parameter(uint8_t *group, uint8_t* channel, uint8_t* key, uint8_t* extra,
                        uint16_t *index, float *value, cmidi2_ump* ump) {
    if (cmidi2_ump_get_message_type(ump) != CMIDI2_MESSAGE_TYPE_SYSEX8_MDS)
        return false;
    auto raw = (uint32_t*) ump;
    return aapReadMidi2ParameterSysex8(group, channel, key, extra, index, value,
                                 *raw, *(raw + 1), *(raw + 2), *(raw + 3));
}

void sample_plugin_process(AndroidAudioPlugin *plugin,
                           aap_buffer_t *buffer,
                           int32_t frameCount,
                           int64_t timeoutInNanoseconds) {

    auto context = (AyumiHandle*) plugin->plugin_specific;
    if (!context->active)
        return;

    volatile auto aapmb = (AAPMidiBufferHeader*) buffer->get_buffer(*buffer, context->midi2_in_port);

    uint32_t currentTicks = 0;

    auto outL = (float*) buffer->get_buffer(*buffer, context->audio_out_l_port);
    auto outR = (float*) buffer->get_buffer(*buffer, context->audio_out_r_port);

    auto midi2ptr = ((uint32_t*) (void*) aapmb) + 8;
    CMIDI2_UMP_SEQUENCE_FOREACH(midi2ptr, aapmb->length, ev) {
        auto ump = (cmidi2_ump *) ev;
        uint8_t paramGroup, paramChannel, paramKey{0}, paramExtra{0};
        uint16_t paramIndex;
        float paramValue;
        uint32_t intValue;
        bool relative{false};
        if (cmidi2_ump_get_message_type(ump) == CMIDI2_MESSAGE_TYPE_UTILITY &&
            cmidi2_ump_get_status_code(ump) == CMIDI2_JR_TIMESTAMP) {
            uint32_t max = currentTicks + (uint32_t) (cmidi2_ump_get_jr_timestamp_timestamp(ump) / 31250.0 * context->sample_rate);
            auto numFrames = buffer->num_frames(*buffer);
            max = max < numFrames ? max : numFrames;
            for (uint32_t i = currentTicks; i < max; i++) {
                ayumi_process(context->impl);
                ayumi_remove_dc(context->impl);
                outL[i] = (float) context->impl->left;
                outR[i] = (float) context->impl->right;
            }
            currentTicks = max;
            continue;
        } else if (readMidi2Parameter(&paramGroup, &paramChannel, &paramKey, &paramExtra, &paramIndex, &paramValue, ump)) {
            intValue = (int32_t) *(float*) &paramValue;
        } else if (cmidi2_ump_get_message_type(ump) == CMIDI2_MESSAGE_TYPE_MIDI_2_CHANNEL) {
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
                    // FIXME: fully down-convert to MIDI1 and process it (sysex can be lengthier)
                    uint8_t midi1Bytes[16];
                    if (cmidi2_convert_single_ump_to_midi1(midi1Bytes, 16, ump) > 0)
                        ayumi_aap_process_midi_event(context, midi1Bytes);
                    continue;
            }
            paramGroup = cmidi2_ump_get_group(ump);
            paramChannel = cmidi2_ump_get_channel(ump);
            paramIndex = cmidi2_ump_get_midi2_nrpn_msb(ump) * 0x80 + cmidi2_ump_get_midi2_nrpn_lsb(ump);
            intValue = cmidi2_ump_get_midi2_nrpn_data(ump);
            paramValue = *(float*) &intValue;
        } else {
            // FIXME: fully down-convert to MIDI1 and process it (sysex can be lengthier)
            uint8_t midi1Bytes[16];
            if (cmidi2_convert_single_ump_to_midi1(midi1Bytes, 16, ump) > 0)
                ayumi_aap_process_midi_event(context, midi1Bytes);
            continue;
        }

        // process parameter changes
        switch (paramIndex & 0xFF) {
            case CMIDI2_CC_BANK_SELECT: {
                auto mixer = intValue & 0xFF;
                auto tone_switch = mixer & 1;
                auto noise_switch = (mixer >> 1) & 1;
                auto env_switch = (mixer >> 2) & 1;
                context->mixer[paramChannel] = mixer << 5;
                ayumi_set_mixer(context->impl, paramChannel, tone_switch, noise_switch,
                                env_switch);
                break;
            }
            case CMIDI2_CC_PAN: {
                float pan = *(float*) &paramValue; // 0.0..1.0
                ayumi_set_pan(context->impl, paramChannel, pan, 0);
                break;
            }
            case CMIDI2_CC_VOLUME: {
                auto volume = (int32_t) *(float*) &paramValue; // there is no valur range mapping for this parameter.
                ayumi_set_volume(context->impl, paramChannel, volume);
                break;
            }
            case AYUMI_AAP_PARAM_ENVELOPE: {
                auto env = ((int32_t) *(float *) &paramValue) & 0xFFFF;
                context->envelope = env;
                ayumi_set_envelope(context->impl, context->envelope);
                break;
            }
            case AYUMI_AAP_MIDI_CC_ENVELOPE_SHAPE:
                auto shape = (int32_t) *(float*) &paramValue;
                ayumi_set_envelope_shape(context->impl, shape);
                break;
        }
    }

    auto numFrames = buffer->num_frames(*buffer);
    if (frameCount > numFrames) {
        aap::a_log_f(AAP_LOG_LEVEL_ERROR, AAP_APP_LOG_TAG, "frameCount passed at process() is bigger than aap_buffer_t num_frames().");
        numFrames = frameCount;
    }
    for (int32_t i = currentTicks; i < numFrames; i++) {
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

size_t sample_plugin_get_state_size(aap_state_extension_t* ext, AndroidAudioPlugin* plugin) {
    // FIXME: implement
    return 0;
}

void sample_plugin_get_state(aap_state_extension_t* ext, AndroidAudioPlugin* plugin, aap_state_t* state) {
    // FIXME: implement
}

void sample_plugin_set_state(aap_state_extension_t* ext, AndroidAudioPlugin* plugin, aap_state_t* input) {
    // FIXME: implement
}

aap_state_extension_t state_extension{nullptr,
                                      sample_plugin_get_state_size,
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

int32_t sample_plugin_get_preset_count(aap_presets_extension_t* ext, AndroidAudioPlugin* /*plugin*/) {
    return sizeof(presets) / sizeof(aap_preset_t);
}

int32_t sample_plugin_get_preset_data_size(aap_presets_extension_t* ext, AndroidAudioPlugin* /*plugin*/, int32_t index) {
    return presets[index].data_size; // just for testing, no actual content.
}

void sample_plugin_get_preset(aap_presets_extension_t* ext, AndroidAudioPlugin* /*plugin*/, int32_t index, bool skipContent, aap_preset_t* preset) {
    preset->index = index;
    strncpy(preset->name, presets[index].name, AAP_PRESETS_EXTENSION_MAX_NAME_LENGTH);
    preset->data_size = presets[index].data_size;
    if (!skipContent && preset->data_size > 0)
        memcpy(preset->data, presets[index].data, preset->data_size);
}

int32_t sample_plugin_get_preset_index(aap_presets_extension_t* ext, AndroidAudioPlugin* plugin) {
    auto handle = (AyumiHandle*) plugin->plugin_specific;
    return handle->preset_index;
}

void sample_plugin_set_preset_index(aap_presets_extension_t* ext, AndroidAudioPlugin* plugin, int32_t index) {
    auto handle = (AyumiHandle*) plugin->plugin_specific;
    aap::a_log_f(AAP_LOG_LEVEL_INFO, AAP_APP_LOG_TAG, "Preset changed to %d", index);
    handle->preset_index = index;
}

aap_presets_extension_t presets_extension{nullptr,
                                          sample_plugin_get_preset_count,
                                          sample_plugin_get_preset_data_size,
                                          sample_plugin_get_preset,
                                          sample_plugin_get_preset_index,
                                          sample_plugin_set_preset_index};

int32_t sample_plugin_gui_create(aap_gui_extension_t* ext, AndroidAudioPlugin* plugin, const char* pluginId, int32_t instanceId, void* audioPluginView) {
    aap::a_log(AAP_LOG_LEVEL_INFO, AAP_APP_LOG_TAG, "!!!!! GUI CREATE !!!");
    return 1;
}

int32_t sample_plugin_gui_show(aap_gui_extension_t* ext, AndroidAudioPlugin* plugin, int32_t guiInstanceId) {
    aap::a_log(AAP_LOG_LEVEL_INFO, AAP_APP_LOG_TAG, "!!!!! GUI SHOW !!!");
    return AAP_GUI_RESULT_OK;
}

void sample_plugin_gui_hide(aap_gui_extension_t* ext, AndroidAudioPlugin* plugin, int32_t guiInstanceId) {
    aap::a_log(AAP_LOG_LEVEL_INFO, AAP_APP_LOG_TAG, "!!!!! GUI HIDE !!!");
}

int32_t sample_plugin_gui_resize(aap_gui_extension_t* ext, AndroidAudioPlugin* plugin, int32_t guiInstanceId, int32_t width, int32_t height) {
    aap::a_log(AAP_LOG_LEVEL_INFO, AAP_APP_LOG_TAG, "!!!!! GUI RESIZE !!!");
    return AAP_GUI_RESULT_OK;
}

void sample_plugin_gui_destroy(aap_gui_extension_t* ext, AndroidAudioPlugin* plugin, int32_t guiInstanceId) {
    aap::a_log(AAP_LOG_LEVEL_INFO, AAP_APP_LOG_TAG, "!!!!! GUI DESTROY !!!");
}

aap_gui_extension_t gui_extension{nullptr,
                                  sample_plugin_gui_create,
                                  sample_plugin_gui_show,
                                  sample_plugin_gui_hide,
                                  sample_plugin_gui_resize,
                                  sample_plugin_gui_destroy};

void* sample_plugin_get_extension(AndroidAudioPlugin* plugin, const char *uri) {
    if (strcmp(uri, AAP_STATE_EXTENSION_URI) == 0)
        return &state_extension;
    if (strcmp(uri, AAP_PRESETS_EXTENSION_URI) == 0)
        return &presets_extension;
    if (strcmp(uri, AAP_GUI_EXTENSION_URI) == 0)
        return &gui_extension;
    return nullptr;
}

AndroidAudioPlugin *sample_plugin_new(
        AndroidAudioPluginFactory *pluginFactory,
        const char *pluginUniqueId,
        int sampleRate,
        AndroidAudioPluginHost *host) {

    auto handle = new AyumiHandle();
    handle->host = *host;
    handle->plugin_id = pluginUniqueId;
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

    handle->midi_protocol = 2; // this is for testing MIDI2 in port.

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

extern "C" AndroidAudioPluginFactory *GetAndroidAudioPluginFactory() {
    return &factory;
}
