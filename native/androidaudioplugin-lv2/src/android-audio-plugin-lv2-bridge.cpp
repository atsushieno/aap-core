
#include <unistd.h>
#include <dlfcn.h>
#include <cmath>
#include <cstring>
#include <cassert>
#include <vector>
#include <map>
#if ANDROID
#include <android/log.h>
#endif

#include <lilv/lilv.h>
// FIXME: It is HACK, may not work under some environment...
#include <../lib/lv2/atom.lv2/atom.h>
#include <../lib/lv2/atom.lv2/util.h>
#include <../lib/lv2/urid.lv2/urid.h>
#include <../lib/lv2/midi.lv2/midi.h>
#include <../lib/lv2/log.lv2/log.h>
#include <../lib/lv2/buf-size.lv2/buf-size.h>
#include "aap/android-audio-plugin.h"


namespace aaplv2bridge {

typedef struct {
    bool operator() (const char* p1, const char* p2) const { return strcmp (p1, p2) == 0; }
} uricomp;

// WARNING: NEVER EVER use this function and URID feature variable for loading and saving state.
// State must be preserved in stable semantics, and this function and internal map are never
// stable. The value of the mapped integers change every time we make changes to this code.
LV2_URID urid_map_func (LV2_URID_Map_Handle handle, const char *uri)
{
    auto map = static_cast<std::map<const char*,LV2_URID>*> (handle);
    auto it = map->find (uri);
    if (it == map->end())
        map->emplace (uri, map->size() + 1);
    auto ret = map->find (uri)->second;
    return ret;
}

int avprintf(const char *fmt, va_list ap)
{
#if ANDROID
    return __android_log_print(ANDROID_LOG_INFO, "AAPHostNative", fmt, ap);
#else
    return vprintf(fmt, ap);
#endif
}

int aprintf (const char *fmt,...)
{
    va_list ap;
    va_start (ap, fmt);
    return avprintf(fmt, ap);
}

void aputs(const char* s)
{
#if ANDROID
    __android_log_print(ANDROID_LOG_INFO, "AAPHostNative", "%s", s);
#else
	puts(s);
#endif
}

int log_vprintf (LV2_Log_Handle handle, LV2_URID type, const char *fmt, va_list ap)
{
    int ret = aprintf ("LV2 LOG (%d): ", type);
    ret += aprintf (fmt, ap);
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
	~AAPLV2PluginContext() {
		for (auto p : midi_atom_buffers)
			free(p.second);
	}

    AAPLV2PluginContextStatics *statics;
    LilvWorld *world;
	const LilvPlugin *plugin;
	LilvInstance *instance;
	AndroidAudioPluginBuffer* cached_buffer;
	void* dummy_raw_buffer;
	int32_t midi_buffer_size = 1024;
	std::map<int32_t, LV2_Atom_Sequence*> midi_atom_buffers{};
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
			ctx->midi_atom_buffers.insert_or_assign(p.first, (LV2_Atom_Sequence*) calloc(buffer->num_frames, 1));
		}
	}

	int numPorts = lilv_plugin_get_num_ports (lilvPlugin);
	for (int p = 0; p < numPorts; p++) {
		auto iter = ctx->midi_atom_buffers.find(p);
		auto bp = iter != ctx->midi_atom_buffers.end() ? iter->second : buffer->buffers[p];
		if (bp == NULL)
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

LV2_URID urid_atom_sequence_type{0}, urid_midi_event_type{0};

int32_t normalize_midi_event_for_lv2(void* dst, int32_t dstCapacity, void* src)
{
	// converts AAP MIDI buffers into LV2 MIDI buffer, where both are both MIDI buffers
	// but LV2 MIDI needs to meet these constraints:
	//
	// - Running status is not allowed, every message must have its own status byte.
	// - Note On messages with velocity 0 are not allowed. These messages are equivalent to Note Off in standard MIDI streams, but here only proper Note Off messages are allowed.
	// - "Realtime messages" (status bytes 0xF8 to 0xFF) are allowed, but may not occur inside other messages like they can in standard MIDI streams.
	// - All messages are complete valid MIDI messages. This means, for example, that only the first byte in each event (the status byte) may have the eighth bit set, that Note On and Note Off events are always 3 bytes long, etc. Where messages are communicated, the writer is responsible for writing valid messages, and the reader may assume that all events are valid.
	//
	// And AAP MIDI buffer is a length-prepended raw MIDI buffer with no constraint.

	// FIXME: implement above.

	assert(src != nullptr);
	assert(dst != nullptr);
	int32_t srcSize = ((int*) src)[0];
	int32_t copiedSize = srcSize - dstCapacity < 0 ? srcSize : dstCapacity;
	memcpy(dst, (int*) src + 1, copiedSize);
	return copiedSize;
}

void convert_aap_midi_to_lv2_midi(LV2_Atom_Sequence* dst, int32_t dstCapacity, void* src)
{

	LV2_Atom* atom = (LV2_Atom*) (void*) dst;
	atom->type = urid_atom_sequence_type;
	atom->size = normalize_midi_event_for_lv2(dst + sizeof(dst->atom), dstCapacity - sizeof(dst->atom), src);
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
		LV2_Atom_Sequence *dst = p.second;
		convert_aap_midi_to_lv2_midi (dst, buffer->num_frames, src);
	}

	lilv_instance_run(ctx->instance, buffer->num_frames);
}

void aap_lv2_plugin_deactivate(AndroidAudioPlugin *plugin)
{
	auto l = (AAPLV2PluginContext*) plugin->plugin_specific;
	lilv_instance_deactivate(l->instance);
}

AndroidAudioPluginState state;
const AndroidAudioPluginState* aap_lv2_plugin_get_state(AndroidAudioPlugin *plugin)
{
    return &state; /* fill it */
}

void aap_lv2_plugin_set_state(AndroidAudioPlugin *plugin, AndroidAudioPluginState *input)
{
    /* apply argument input */
}

AndroidAudioPlugin* aap_lv2_plugin_new(
        AndroidAudioPluginFactory *pluginFactory,
        const char* pluginUniqueID,
        int sampleRate,
        const AndroidAudioPluginExtension * const *extensions)
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
    LV2_Feature* features [5];
    std::map<const char*,LV2_URID,uricomp> urid_map;
    LV2_URID_Map mapData{ &urid_map, urid_map_func };
    LV2_Log_Log logData { NULL, log_printf, log_vprintf };
    LV2_Feature uridFeature = { LV2_URID_MAP_URI, &mapData };
    LV2_Feature logFeature = { LV2_LOG_URI, &logData };
    LV2_Feature bufSizeFeature = { LV2_BUF_SIZE_URI, NULL };
    LV2_Feature atomFeature = { LV2_ATOM_URI, NULL };
    // FIXME: convert those AAP extensions to LV2 features
    features [0] = &uridFeature;
    features [1] = &logFeature;
    features [2] = &bufSizeFeature;
    features [3] = &atomFeature;
    features [4] = NULL;

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
	}

    auto ctx = new AAPLV2PluginContext { statics, world, plugin, instance, NULL, NULL };
    int nPorts = lilv_plugin_get_num_ports(plugin);
	for (int i = 0; i < nPorts; i++) {
		if (IS_ATOM_PORT(ctx, plugin, lilv_plugin_get_port_by_index(plugin, i))) {
			ctx->midi_atom_buffers.insert_or_assign(i, (LV2_Atom_Sequence*) calloc(ctx->midi_buffer_size, 1));
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
