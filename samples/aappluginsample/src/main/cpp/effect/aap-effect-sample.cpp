
#include <aap/android-audio-plugin.h>
#include <aap/ext/state.h>
#include <aap/ext/midi.h>
#include <aap/ext/parameters.h>
#include <aap/ext/buses.h>
#include <aap/unstable/logging.h>
#include <cassert>
#include <cstring>
#include "cmidi2.h"

extern "C" {

#define AAP_APP_LOG_TAG "AAPBarebonePluginSample"


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
    // main bus channel counts (1 or 2); changed by the host via apply_layout().
    int32_t numInputChannels{2};
    int32_t numOutputChannels{2};

    SamplePluginSpecific(AndroidAudioPluginHost *host) {
        this->host = *host;
        for (size_t i = 0; i < 128; i++)
            modL_pn[i] = modR_pn[i] = 0.5f;
    }
} SamplePluginSpecific;

typedef struct SamplePluginState {
    uint32_t version;
    float modL;
    float modR;
    uint32_t delayL;
    uint32_t delayR;
    float modL_pn[128];
    float modR_pn[128];
} SamplePluginState;

static constexpr uint32_t SAMPLE_PLUGIN_STATE_VERSION = 1;

void sample_plugin_delete(
        AndroidAudioPluginFactory *pluginFactory,
        AndroidAudioPlugin *instance) {
    delete (SamplePluginSpecific*) instance->plugin_specific;
    delete instance;
}

void sample_plugin_prepare(AndroidAudioPlugin *plugin, int32_t sampleRate, aap_buffer_t *buffer) {
    auto ctx = (SamplePluginSpecific*) plugin->plugin_specific;
    // The bus accessors on aap_buffer_t are available only with the buses host extension.
    // (This plugin is always built with an aap-core that provides it.)
    auto buses = ctx->host.get_extension(&ctx->host, AAP_BUSES_EXTENSION_URI);
    assert(buses);
    (void) buses;
}

void sample_plugin_activate(AndroidAudioPlugin *plugin) {}

static double get_parameter_min(uint16_t index) {
    switch (index) {
        case PARAM_ID_VOLUME_L:
        case PARAM_ID_VOLUME_R:
            return 0.0;
        case PARAM_ID_DELAY_L:
        case PARAM_ID_DELAY_R:
            return 0.0;
        default:
            return 0.0;
    }
}

static double get_parameter_max(uint16_t index) {
    switch (index) {
        case PARAM_ID_VOLUME_L:
        case PARAM_ID_VOLUME_R:
            return 1.0;
        case PARAM_ID_DELAY_L:
        case PARAM_ID_DELAY_R:
            return 2048.0;
        default:
            return 1.0;
    }
}

bool readMidi2Parameter(uint8_t *group, uint8_t* channel, uint8_t* key, uint8_t* extra, uint16_t *index, double *value, cmidi2_ump* ump) {
    if (cmidi2_ump_get_message_type(ump) != CMIDI2_MESSAGE_TYPE_SYSEX8_MDS)
        return false;
    auto raw = (uint32_t*) ump;
    uint32_t transportValue;
    auto result = aapReadMidi2ParameterSysex8(group, channel, key, extra, index, &transportValue, *raw, *(raw + 1), *(raw + 2), *(raw + 3));
    if (result)
        *value = aapParameterTransportUint32ToPlain(get_parameter_min(*index), get_parameter_max(*index), transportValue);
    return result;
}

void sample_plugin_process(AndroidAudioPlugin *plugin,
                           aap_buffer_t *buffer,
                           int32_t frameCount,
                           int64_t timeoutInNanoseconds) {
    // apply super-simple delay processing with super-simple volume adjustment per channel.

    auto ctx = (SamplePluginSpecific*) plugin->plugin_specific;

    int size = buffer->num_frames(buffer) * sizeof(float);
    if (frameCount > size) {
        aap::a_log_f(AAP_LOG_LEVEL_ERROR, AAP_APP_LOG_TAG, "frameCount passed at process() is bigger than aap_buffer_t num_frames().");
        size = frameCount;
    }

    // The main buses. A host may give us mono buses (e.g. an older host that assumed defaults).
    auto numIns = buffer->get_audio_channel_count(buffer, AAP_PORT_DIRECTION_INPUT, 0);
    auto numOuts = buffer->get_audio_channel_count(buffer, AAP_PORT_DIRECTION_OUTPUT, 0);
    if (numIns == 0 || numOuts == 0)
        return;
    auto ins = buffer->get_audio_channels(buffer, AAP_PORT_DIRECTION_INPUT, 0);
    auto outs = buffer->get_audio_channels(buffer, AAP_PORT_DIRECTION_OUTPUT, 0);
    auto fIL = ins[0];
    auto fIR = numIns > 1 ? ins[1] : ins[0];
    auto fOL = outs[0];
    auto fOR = numOuts > 1 ? outs[1] : outs[0];

    // update parameters via MIDI2 messages
    auto midiSeq = (AAPMidiBufferHeader*) buffer->get_event_buffer(buffer, AAP_PORT_DIRECTION_INPUT, 0);
    auto midiSeqData = midiSeq + 1;
    if (midiSeq->length > 0) {
        CMIDI2_UMP_SEQUENCE_FOREACH(midiSeqData, midiSeq->length, iter) {
            auto ump = (cmidi2_ump*) iter;
            uint8_t paramGroup, paramChannel, paramKey{0}, paramExtra{0};
            uint16_t paramIndex;
            double paramValue;
            uint32_t transportValue{0};
            bool relative{false};
            switch (cmidi2_ump_get_message_type(ump)) {
                case CMIDI2_MESSAGE_TYPE_UTILITY:
                    if (cmidi2_ump_get_status_code(ump) == CMIDI2_UTILITY_STATUS_JR_TIMESTAMP) {
                        // FIXME: ideally there should be timestamp-based sample accuracy adjustment here
                        //  and the audio processing code later should take care of it, but we're lazy here...
                    }
                    continue;
                case CMIDI2_MESSAGE_TYPE_SYSEX8_MDS: {
                    if (!readMidi2Parameter(&paramGroup, &paramChannel, &paramKey, &paramExtra,
                                            &paramIndex, &paramValue, ump))
                        continue;
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
                    transportValue = cmidi2_ump_get_midi2_nrpn_data(ump);
                    paramValue = aapParameterTransportUint32ToPlain(get_parameter_min(paramIndex), get_parameter_max(paramIndex), transportValue);
                } break;
            }
            switch (paramIndex) {
                case PARAM_ID_VOLUME_L:
                    if (paramKey != 0)
                        ctx->modL_pn[paramKey] = static_cast<float>(relative ? ctx->modL_pn[paramKey] + paramValue : paramValue);
                    else
                        ctx->modL = static_cast<float>(relative ? ctx->modL + paramValue : paramValue);
                    break;
                case PARAM_ID_VOLUME_R:
                    if (paramKey != 0)
                        ctx->modR_pn[paramKey] = static_cast<float>(relative ? ctx->modR_pn[paramKey] + paramValue : paramValue);
                    else
                        ctx->modR = static_cast<float>(relative ? ctx->modR + paramValue : paramValue);
                    break;
                case PARAM_ID_DELAY_L:
                    if (paramKey != 0)
                        break; // FIXME: implement or log it?
                    ctx->delayL = static_cast<uint32_t>(paramValue) + (relative ? ctx->delayL : 0);
                    break;
                case PARAM_ID_DELAY_R:
                    if (paramKey != 0)
                        break; // FIXME: implement or log it?
                    ctx->delayR = static_cast<uint32_t>(paramValue) + (relative ? ctx->delayR : 0);
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

// state extension

size_t sample_plugin_get_state_size(aap_state_extension_t* ext, AndroidAudioPlugin* plugin) {
    return sizeof(SamplePluginState);
}

void sample_plugin_get_state(aap_state_extension_t* ext, AndroidAudioPlugin* plugin, aap_state_t* state) {
    auto ctx = (SamplePluginSpecific*) plugin->plugin_specific;
    auto output = (SamplePluginState*) state->data;
    output->version = SAMPLE_PLUGIN_STATE_VERSION;
    output->modL = ctx->modL;
    output->modR = ctx->modR;
    output->delayL = ctx->delayL;
    output->delayR = ctx->delayR;
    memcpy(output->modL_pn, ctx->modL_pn, sizeof(output->modL_pn));
    memcpy(output->modR_pn, ctx->modR_pn, sizeof(output->modR_pn));
    state->data_size = sizeof(SamplePluginState);
}

void sample_plugin_set_state(aap_state_extension_t* ext, AndroidAudioPlugin* plugin, aap_state_t* input) {
    if (!input || !input->data || input->data_size < sizeof(SamplePluginState))
        return;

    auto ctx = (SamplePluginSpecific*) plugin->plugin_specific;
    auto state = (SamplePluginState*) input->data;
    if (state->version != SAMPLE_PLUGIN_STATE_VERSION)
        return;

    ctx->modL = state->modL;
    ctx->modR = state->modR;
    ctx->delayL = state->delayL;
    ctx->delayR = state->delayR;
    memcpy(ctx->modL_pn, state->modL_pn, sizeof(ctx->modL_pn));
    memcpy(ctx->modR_pn, state->modR_pn, sizeof(ctx->modR_pn));
}

aap_state_extension_t state_extension{nullptr,
                                      sample_plugin_get_state_size,
                                      sample_plugin_get_state,
                                      sample_plugin_set_state};

// parameters extension

int32_t sample_plugin_get_parameter_count(aap_parameters_extension_t* ext, AndroidAudioPlugin* plugin) {
    return 8;
}

aap_parameter_info_t parameter_infos[] {
        {0, "Output Volume L", "", 0.0, 1.0, 0.5},
        {1, "Output Volume R", "", 0.0, 1.0, 0.5},
        {2, "Delay L", "", 0, 2048, 0},
        {3, "Delay R", "", 0, 2048, 256},
        {11, "Stub Parameter 4", "", 0, 1,0 },
        {12, "Stub Parameter 5", "", 0, 1,0 },
        {13, "Stub Parameter 6", "", 0, 1,0 },
        {14, "Stub Parameter 7", "", 0, 1,0 },
};

aap_parameter_info_t sample_plugin_get_parameter(aap_parameters_extension_t* ext, AndroidAudioPlugin* plugin, int32_t index) {
    return parameter_infos[index];
}

aap_parameters_extension_t parameters_extension{nullptr,
                                                sample_plugin_get_parameter_count,
                                                sample_plugin_get_parameter,
                                                nullptr,
                                                nullptr,
                                                nullptr};

// Buses extension: a main input and a main output, either stereo (default) or mono.
// (The framework adds the main event buses.)
int32_t sample_plugin_get_bus_count(aap_buses_extension_t* ext, AndroidAudioPlugin* plugin,
                                    aap_bus_kind kind, aap_port_direction direction) {
    return kind == AAP_BUS_KIND_AUDIO ? 1 : 0;
}

aap_bus_info_t sample_plugin_get_bus(aap_buses_extension_t* ext, AndroidAudioPlugin* plugin,
                                     aap_bus_kind kind, aap_port_direction direction, int32_t index) {
    auto ctx = (SamplePluginSpecific*) plugin->plugin_specific;
    aap_bus_info_t bus{};
    bus.id = direction == AAP_PORT_DIRECTION_INPUT ? 0 : 1;
    bus.kind = AAP_BUS_KIND_AUDIO;
    bus.direction = direction;
    bus.role = AAP_BUS_ROLE_MAIN;
    strncpy(bus.name, direction == AAP_PORT_DIRECTION_INPUT ? "Audio In" : "Audio Out", AAP_MAX_BUS_NAME_CHARS - 1);
    bus.channel_count = direction == AAP_PORT_DIRECTION_INPUT ? ctx->numInputChannels : ctx->numOutputChannels;
    strncpy(bus.layout, bus.channel_count == 1 ? "mono" : "stereo", AAP_MAX_BUS_LAYOUT_CHARS - 1);
    bus.enabled = true;
    return bus;
}

bool sample_plugin_apply_layout(aap_buses_extension_t* ext, AndroidAudioPlugin* plugin,
                                const aap_bus_layout_request_t* request) {
    auto ctx = (SamplePluginSpecific*) plugin->plugin_specific;
    int32_t numIns = ctx->numInputChannels, numOuts = ctx->numOutputChannels;
    for (int32_t i = 0; i < request->count; i++) {
        auto& bus = request->buses[i];
        if (bus.id > 1 || !bus.enabled || bus.channel_count < 1 || bus.channel_count > 2)
            return false;
        (bus.id == 0 ? numIns : numOuts) = bus.channel_count;
    }
    ctx->numInputChannels = numIns;
    ctx->numOutputChannels = numOuts;
    return true;
}

aap_buses_extension_t buses_extension{nullptr,
                                      sample_plugin_get_bus_count,
                                      sample_plugin_get_bus,
                                      sample_plugin_apply_layout};

void* sample_plugin_get_extension(AndroidAudioPlugin *, const char* uri) {
    if (!strcmp(uri, AAP_PARAMETERS_EXTENSION_URI))
        return &parameters_extension;
    if (!strcmp(uri, AAP_BUSES_EXTENSION_URI))
        return &buses_extension;
    if (!strcmp(uri, AAP_STATE_EXTENSION_URI))
        return &state_extension;
    return nullptr;
}

AndroidAudioPlugin *sample_plugin_new(
        AndroidAudioPluginFactory *pluginFactory,
        const char *pluginUniqueId,
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
