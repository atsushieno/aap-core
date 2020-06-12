
#include <unistd.h>
#include <dlfcn.h>
#include <cmath>
#include <cstring>
#include <cassert>
#include <vector>
#include <map>
#include <string>

#include "aap/logging.h"
#include "aap/android-audio-plugin.h"

#include <lilv/lilv.h>
#include <lv2/atom/atom.h>
#include <lv2/atom/util.h>
#include <lv2/atom/forge.h>
#include <lv2/urid/urid.h>
#include <lv2/midi/midi.h>
#include <lv2/time/time.h>
#include <lv2/log/log.h>
#include <lv2/buf-size/buf-size.h>


namespace aaplv2bridge {

typedef struct {
    bool operator() (std::string p1, std::string p2) const { return p1 == p2; }
} uricomp;

// WARNING: NEVER EVER use this function and URID feature variable for loading and saving state.
// State must be preserved in stable semantics, and this function and internal map are never
// stable. The value of the mapped integers change every time we make changes to this code.
LV2_URID urid_map_func (LV2_URID_Map_Handle handle, const char *uri)
{
	auto map = static_cast<std::map<std::string,LV2_URID>*> (handle);
    std::string s{uri};
    auto it = map->find (s);
    if (it == map->end())
        map->emplace (s, map->size() + 1000);
    return map->find(s)->second;
}

int log_vprintf (LV2_Log_Handle handle, LV2_URID type, const char *fmt, va_list ap)
{
    int ret = aap::aprintf ("LV2 LOG (%d): ", type);
    ret += aap::aprintf (fmt, ap);
    return ret;
}

int log_printf (LV2_Log_Handle handle, LV2_URID type, const char *fmt,...)
{
    va_list ap;
    va_start (ap, fmt);
    return log_vprintf (handle, type, fmt, ap);
}

typedef struct {
    LilvNode *audio_port_uri_node, *control_port_uri_node, *atom_port_uri_node, *input_port_uri_node, *output_port_uri_node;

} AAPLV2PluginContextStatics;

class AAPLV2PluginContext {
public:
	AAPLV2PluginContext(AAPLV2PluginContextStatics* statics, LilvWorld* world, const LilvPlugin* plugin, LilvInstance* instance)
		: statics(statics), world(world), plugin(plugin), instance(instance)
	{
		midi_atom_forge = (LV2_Atom_Forge*) calloc(1024, 1);
	}

	~AAPLV2PluginContext() {
		for (auto p : midi_atom_buffers)
			free(p.second);
		free(midi_atom_forge);
	}

    AAPLV2PluginContextStatics *statics;
    LilvWorld *world;
	const LilvPlugin *plugin;
	LilvInstance *instance;
	AndroidAudioPluginBuffer* cached_buffer{nullptr};
	void* dummy_raw_buffer{nullptr};
	int32_t midi_buffer_size = 1024;
	std::map<int32_t, LV2_Atom_Sequence*> midi_atom_buffers{};
	LV2_Atom_Forge *midi_atom_forge;
};

#define PORTCHECKER_SINGLE(_name_,_type_) inline bool _name_ (AAPLV2PluginContext *ctx, const LilvPlugin* plugin, const LilvPort* port) { return lilv_port_is_a (plugin, port, ctx->statics->_type_); }
#define PORTCHECKER_AND(_name_,_cond1_,_cond2_) inline bool _name_ (AAPLV2PluginContext *ctx, const LilvPlugin* plugin, const LilvPort* port) { return _cond1_ (ctx, plugin, port) && _cond2_ (ctx, plugin, port); }

    PORTCHECKER_SINGLE (IS_AUDIO_PORT, audio_port_uri_node)
    PORTCHECKER_SINGLE (IS_INPUT_PORT, input_port_uri_node)
    PORTCHECKER_SINGLE (IS_OUTPUT_PORT, output_port_uri_node)
    PORTCHECKER_SINGLE (IS_CONTROL_PORT, control_port_uri_node)
	PORTCHECKER_SINGLE (IS_ATOM_PORT, atom_port_uri_node)
    PORTCHECKER_AND (IS_AUDIO_IN, IS_AUDIO_PORT, IS_INPUT_PORT)
    PORTCHECKER_AND (IS_AUDIO_OUT, IS_AUDIO_PORT, IS_OUTPUT_PORT)
    PORTCHECKER_AND (IS_CONTROL_IN, IS_CONTROL_PORT, IS_INPUT_PORT)
    PORTCHECKER_AND (IS_CONTROL_OUT, IS_CONTROL_PORT, IS_OUTPUT_PORT)
	PORTCHECKER_AND (IS_ATOM_IN, IS_ATOM_PORT, IS_INPUT_PORT)
	PORTCHECKER_AND (IS_ATOM_OUT, IS_ATOM_PORT, IS_OUTPUT_PORT)


void aap_lv2_plugin_delete(
        AndroidAudioPluginFactory *pluginFactory,
        AndroidAudioPlugin *plugin)
{
    auto l = (AAPLV2PluginContext*) plugin->plugin_specific;
	free(l->dummy_raw_buffer);
    lilv_instance_free(l->instance);
    lilv_node_free(l->statics->audio_port_uri_node);
    lilv_node_free(l->statics->control_port_uri_node);
	lilv_node_free(l->statics->atom_port_uri_node);
    lilv_node_free(l->statics->input_port_uri_node);
    lilv_node_free(l->statics->output_port_uri_node);
	delete l->statics;
	lilv_world_free(l->world);
    delete l;
    delete plugin;
}

void resetPorts(AndroidAudioPlugin *plugin, AndroidAudioPluginBuffer* buffer)
{
	auto ctx = (AAPLV2PluginContext*) plugin->plugin_specific;
	auto lilvPlugin = ctx->plugin;
	auto instance = ctx->instance;
	
	auto dummyBuffer = calloc(buffer->num_frames * sizeof(float), 1);
	ctx->dummy_raw_buffer = dummyBuffer;
	
	ctx->cached_buffer = buffer;

	assert(buffer != nullptr);

	// FIXME: it is quite awkward to reset buffer size to whatever value for audio I/O but
	//  there is no any reasonable alternative value to reuse. Maybe something like 0x1000 is enough
	//  (but what happens if there are MPE-like massive messages?)
	if (ctx->midi_buffer_size < buffer->num_frames) {
		for (auto p : ctx->midi_atom_buffers) {
			free(p.second);
			ctx->midi_atom_buffers[p.first] = (LV2_Atom_Sequence*) calloc(buffer->num_frames, 1);
		}
	}

	int numPorts = lilv_plugin_get_num_ports (lilvPlugin);
	for (int p = 0; p < numPorts; p++) {
		auto iter = ctx->midi_atom_buffers.find(p);
		auto bp = iter != ctx->midi_atom_buffers.end() ? iter->second : buffer->buffers[p];
		if (bp == nullptr)
			lilv_instance_connect_port (instance, p, dummyBuffer);
		else
			lilv_instance_connect_port (instance, p, bp);
	}
}

void aap_lv2_plugin_prepare(AndroidAudioPlugin *plugin, AndroidAudioPluginBuffer* buffer)
{
	resetPorts(plugin, buffer);
}

void aap_lv2_plugin_activate(AndroidAudioPlugin *plugin)
{
	auto l = (AAPLV2PluginContext*) plugin->plugin_specific;
	lilv_instance_activate(l->instance);
}

std::map<std::string,LV2_URID,uricomp> urid_map{};
LV2_URID_Map urid_map_feature_data{ &urid_map, urid_map_func };
LV2_URID urid_atom_sequence_type{0}, urid_midi_event_type{0}, urid_time_frame{0}, urid_time_beats{0};

// returns true if there was at least one MIDI message in src.
void normalize_midi_event_for_lv2_forge(LV2_Atom_Forge *forge, LV2_Atom_Sequence* seq, int32_t dstCapacity, int timeDivision, void* src) {
	assert(src != nullptr);
	assert(forge != nullptr);

	int srcN = 8;

	auto csrc = (uint8_t*) src;
	int32_t srcEnd = *((int*) src + 1) + 8; // offset

	unsigned char running_status = 0;

	// This is far from precise. No support for sysex and meta, no run length.
	while (srcN < srcEnd) {
		// MIDI Event message
		// Atom Event header
		long timecode = 0;
		int digits = 0;
		while (csrc[srcN] >= 0x80 && srcN < srcEnd) // variable length
			timecode += ((csrc[srcN++] - 0x80) << (7 * digits++));
		if (srcN == srcEnd)
			break; // invalid data
		timecode += (csrc[srcN++] << (7 * digits));

		uint8_t statusByte = csrc[srcN] >= 0x80 ? csrc[srcN] : running_status;
		running_status = statusByte;
		uint8_t eventType = statusByte & 0xF0;
		uint32_t midiEventSize = 3;
		int sysexPos = srcN;
		switch (eventType) {
		case 0xF0:
			midiEventSize = 2; // F0 + F7
			while (csrc[sysexPos++] != 0xF7 && sysexPos < srcEnd)
				midiEventSize++;
			break;
		case 0xC0: case 0xD0: case 0xF1: case 0xF3: case 0xF9: midiEventSize = 2; break;
		case 0xF6: case 0xF7: midiEventSize = 1; break;
		default:
			if (eventType > 0xF8)
				midiEventSize = 1;
			break;
		}

		if(timeDivision > 0x7FFF) {
			// should be "fps" part take into consideration here? framesPerSecond is already specified as numFrames.
			uint8_t ticksPerFrame = timeDivision & 0xFF;
			lv2_atom_forge_frame_time(forge, timecode / ticksPerFrame);
		} else {
			// FIXME: find what kind of semantics LV2 timecode assumes.
			lv2_atom_forge_beat_time(forge, timecode / timeDivision * 120 / 60);
		}
		lv2_atom_forge_raw(forge, &midiEventSize, sizeof(int));
		lv2_atom_forge_raw(forge, &urid_midi_event_type, sizeof(int));
		lv2_atom_forge_raw(forge, csrc + srcN, midiEventSize);
		lv2_atom_forge_pad(forge, midiEventSize);
		srcN += midiEventSize;
	}
}

void aap_lv2_plugin_process(AndroidAudioPlugin *plugin,
                               AndroidAudioPluginBuffer* buffer,
                               long timeoutInNanoseconds)
{
    // FIXME: use timeoutInNanoseconds?

	auto ctx = (AAPLV2PluginContext*) plugin->plugin_specific;
	
	if (buffer != ctx->cached_buffer)
		resetPorts(plugin, buffer);

	// convert AAP MIDI messages into Atom Sequence of MidiEvent.
	for (auto p : ctx->midi_atom_buffers) {
		void *src = buffer->buffers[p.first];
		auto uridMap = &urid_map_feature_data;
		auto forge = ctx->midi_atom_forge;
		lv2_atom_forge_init(forge, uridMap);
		lv2_atom_forge_set_buffer(forge, (uint8_t*) p.second, buffer->num_frames * sizeof(float));
		LV2_Atom_Forge_Frame frame;
		lv2_atom_sequence_clear(p.second);
		int32_t timeDivision = *((int*) src) & 0xFFFF;
		auto seqRef = lv2_atom_forge_sequence_head(forge, &frame, timeDivision > 0x7FFF ? urid_time_frame : urid_time_beats);
		auto seq = (LV2_Atom_Sequence*) lv2_atom_forge_deref(forge, seqRef);
		lv2_atom_forge_pop(forge, &frame);
		normalize_midi_event_for_lv2_forge(forge, seq, buffer->num_frames, timeDivision, src);
		seq->atom.size = forge->offset - sizeof(LV2_Atom);
	}

	lilv_instance_run(ctx->instance, buffer->num_frames);
}

void aap_lv2_plugin_deactivate(AndroidAudioPlugin *plugin)
{
	auto l = (AAPLV2PluginContext*) plugin->plugin_specific;
	lilv_instance_deactivate(l->instance);
}

void aap_lv2_plugin_get_state(AndroidAudioPlugin *plugin, AndroidAudioPluginState* result)
{
	/* FIXME: implement */
}

void aap_lv2_plugin_set_state(AndroidAudioPlugin *plugin, AndroidAudioPluginState *input)
{
	/* FIXME: implement */
}

LV2_Feature* features [6];
LV2_Log_Log logData { NULL, log_printf, log_vprintf };
LV2_Feature uridFeature { LV2_URID__map, &urid_map_feature_data };
LV2_Feature logFeature { LV2_LOG_URI, &logData };
LV2_Feature bufSizeFeature { LV2_BUF_SIZE_URI, NULL };
LV2_Feature atomFeature { LV2_ATOM_URI, NULL };
LV2_Feature timeFeature { LV2_TIME_URI, NULL };

AndroidAudioPlugin* aap_lv2_plugin_new(
        AndroidAudioPluginFactory *pluginFactory,
        const char* pluginUniqueID,
        int sampleRate,
        AndroidAudioPluginExtension ** extensions)
{
	auto world = lilv_world_new();
	// Here we expect that LV2_PATH is already set using setenv() etc.
    lilv_world_load_all (world);

    auto statics = new AAPLV2PluginContextStatics();
    statics->audio_port_uri_node = lilv_new_uri (world, LV2_CORE__AudioPort);
    statics->control_port_uri_node = lilv_new_uri (world, LV2_CORE__ControlPort);
    statics->input_port_uri_node = lilv_new_uri (world, LV2_CORE__InputPort);
    statics->output_port_uri_node = lilv_new_uri (world, LV2_CORE__OutputPort);
	statics->atom_port_uri_node = lilv_new_uri (world, LV2_ATOM__AtomPort);

    auto allPlugins = lilv_world_get_all_plugins (world);
    // FIXME: convert those AAP extensions to LV2 features
    features [0] = &uridFeature;
    features [1] = &logFeature;
    features [2] = &bufSizeFeature;
    features [3] = &atomFeature;
    features [4] = &timeFeature;
	features [5] = NULL;

    // LV2 Plugin URI is just LV2 URI prefixed by "lv2".
    assert (!strncmp(pluginUniqueID, "lv2:", strlen("lv2:")));
    auto pluginUriNode = lilv_new_uri (world, pluginUniqueID + strlen("lv2:"));
    const LilvPlugin *plugin = lilv_plugins_get_by_uri (allPlugins, pluginUriNode);
    lilv_node_free(pluginUriNode);
    assert (plugin);
	LilvInstance *instance = lilv_plugin_instantiate (plugin, sampleRate, features);
    assert (instance);

	// Fixed value list of URID map. If it breaks then saved state will be lost!
	if (!urid_atom_sequence_type) {
		auto map = (LV2_URID_Map*) uridFeature.data;
		urid_atom_sequence_type = map->map(map->handle, LV2_ATOM__Sequence);
		urid_midi_event_type = map->map(map->handle, LV2_MIDI__MidiEvent);
		urid_time_beats = map->map(map->handle, LV2_ATOM__beatTime);
		urid_time_frame = map->map(map->handle, LV2_ATOM__frameTime);
	}

    auto ctx = new AAPLV2PluginContext(statics, world, plugin, instance);

	int nPorts = lilv_plugin_get_num_ports(plugin);
	for (int i = 0; i < nPorts; i++) {
		if (IS_ATOM_PORT(ctx, plugin, lilv_plugin_get_port_by_index(plugin, i))) {
			ctx->midi_atom_buffers[i] = (LV2_Atom_Sequence*) calloc(ctx->midi_buffer_size, 1);
		}
	}

    return new AndroidAudioPlugin {
			ctx,
            aap_lv2_plugin_prepare,
            aap_lv2_plugin_activate,
            aap_lv2_plugin_process,
            aap_lv2_plugin_deactivate,
            aap_lv2_plugin_get_state,
            aap_lv2_plugin_set_state
    };
}

} // namespace aaplv2bridge

AndroidAudioPluginFactory _aap_lv2_factory{aaplv2bridge::aap_lv2_plugin_new, aaplv2bridge::aap_lv2_plugin_delete};

extern "C" {

AndroidAudioPluginFactory *GetAndroidAudioPluginFactoryLV2Bridge() { return &_aap_lv2_factory; }

} // extern "C"
