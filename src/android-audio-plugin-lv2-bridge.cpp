
#include <jni.h>
#include <android/log.h>
#include <unistd.h>
#include <dlfcn.h>
#include <cmath>
#include <cstring>
#include <vector>
#include <map>

#include <lilv/lilv.h>
#include <../lib/lv2/atom.lv2/atom.h>
#include <../lib/lv2/log.lv2/log.h>
#include <../lib/lv2/buf-size.lv2/buf-size.h>
#include "../include/android-audio-plugin.h"


namespace aaplv2bridge {

LilvNode *audio_port_uri_node, *control_port_uri_node, *cv_port_uri_node, *input_port_uri_node, *output_port_uri_node;

#define PORTCHECKER_SINGLE(_name_,_type_) inline bool _name_ (const LilvPlugin* plugin, const LilvPort* port) { return lilv_port_is_a (plugin, port, _type_); }
#define PORTCHECKER_AND(_name_,_cond1_,_cond2_) inline bool _name_ (const LilvPlugin* plugin, const LilvPort* port) { return _cond1_ (plugin, port) && _cond2_ (plugin, port); }

PORTCHECKER_SINGLE (IS_AUDIO_PORT, audio_port_uri_node)
PORTCHECKER_SINGLE (IS_INPUT_PORT, input_port_uri_node)
PORTCHECKER_SINGLE (IS_OUTPUT_PORT, output_port_uri_node)
PORTCHECKER_SINGLE (IS_CONTROL_PORT, control_port_uri_node)
PORTCHECKER_SINGLE (IS_CV_PORT, cv_port_uri_node)
PORTCHECKER_AND (IS_AUDIO_IN, IS_AUDIO_PORT, IS_INPUT_PORT)
PORTCHECKER_AND (IS_AUDIO_OUT, IS_AUDIO_PORT, IS_OUTPUT_PORT)
PORTCHECKER_AND (IS_CONTROL_IN, IS_CONTROL_PORT, IS_INPUT_PORT)
PORTCHECKER_AND (IS_CONTROL_OUT, IS_CONTROL_PORT, IS_OUTPUT_PORT)

typedef struct {
    bool operator() (char* p1, char* p2) { return strcmp (p1, p2) == 0; }
} uricomp;

LV2_URID urid_map_func (LV2_URID_Map_Handle handle, const char *uri)
{
    printf ("-- map %s", uri);
    auto map = static_cast<std::map<const char*,LV2_URID>*> (handle);
    auto it = map->find (uri);
    if (it == map->end())
        map->emplace (uri, map->size() + 1);
    auto ret = map->find (uri)->second;
    printf (" ... as %d\n", ret);
    return ret;
}

int avprintf(const char *fmt, va_list ap)
{
    return __android_log_print(ANDROID_LOG_INFO, "AAPHostNative", fmt, ap);
}

int aprintf (const char *fmt,...)
{
    va_list ap;
    va_start (ap, fmt);
    return avprintf(fmt, ap);
}

void aputs(const char* s)
{
    __android_log_print(ANDROID_LOG_INFO, "AAPHostNative", "%s", s);
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
	LilvWorld *world;
	const LilvPlugin *plugin;
	LilvInstance *instance;
	AndroidAudioPluginBuffer* cached_buffer;
	void* dummy_raw_buffer;
} AAPLV2PluginContext;


void aap_lv2_plugin_delete(
        AndroidAudioPluginFactory *pluginFactory,
        AndroidAudioPlugin *plugin)
{
    auto l = (AAPLV2PluginContext*) plugin->plugin_specific;
	free(l->dummy_raw_buffer);
	delete l;
    delete plugin;
}

void resetPorts(AndroidAudioPlugin *plugin, AndroidAudioPluginBuffer* buffer)
{
	auto l = (AAPLV2PluginContext*) plugin->plugin_specific;
	auto lilvPlugin = l->plugin;
	auto instance = l->instance;
	
	// FIXME: get audio channel count and replace "2"
	auto dummyBuffer = calloc(buffer->num_frames * 2 * sizeof(float), 1);
	l->dummy_raw_buffer = dummyBuffer;
	
	l->cached_buffer = buffer;
	if (buffer) {
        int numPorts = lilv_plugin_get_num_ports (lilvPlugin);
        for (int p = 0; p < numPorts; p++) {
			auto bp = buffer->buffers[p];
			if (bp == NULL)
				lilv_instance_connect_port (instance, p, dummyBuffer);
			else
				lilv_instance_connect_port (instance, p, bp);
        }
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

void aap_lv2_plugin_process(AndroidAudioPlugin *plugin,
                               AndroidAudioPluginBuffer* buffer,
                               long timeoutInNanoseconds)
{
    // FIXME: use timeoutInNanoseconds?

	auto l = (AAPLV2PluginContext*) plugin->plugin_specific;
	
	if (buffer != l->cached_buffer)
		resetPorts(plugin, buffer);

	lilv_instance_run(l->instance, buffer->num_frames);
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

    audio_port_uri_node = lilv_new_uri (world, LV2_CORE__AudioPort);
    control_port_uri_node = lilv_new_uri (world, LV2_CORE__ControlPort);
    input_port_uri_node = lilv_new_uri (world, LV2_CORE__InputPort);
    output_port_uri_node = lilv_new_uri (world, LV2_CORE__OutputPort);

    auto allPlugins = lilv_world_get_all_plugins (world);
    LV2_Feature* features [5];
    auto urid_map = new std::map<char*,LV2_URID,uricomp> ();
    auto mapData = LV2_URID_Map { urid_map, urid_map_func };
    auto logData = LV2_Log_Log { NULL, log_printf, log_vprintf };
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
    assert (plugin);
	LilvInstance *instance = lilv_plugin_instantiate (plugin, sampleRate, features);
    assert (instance);    

    return new AndroidAudioPlugin {
			new AAPLV2PluginContext { world, plugin, instance, NULL, NULL },
            aap_lv2_plugin_prepare,
            aap_lv2_plugin_activate,
            aap_lv2_plugin_process,
            aap_lv2_plugin_deactivate,
            aap_lv2_plugin_get_state,
            aap_lv2_plugin_set_state
    };
}

} // namespace aaplv2bridge

extern "C" {

AndroidAudioPluginFactory *GetAndroidAudioPluginFactoryLV2Bridge() {
    return new AndroidAudioPluginFactory{aaplv2bridge::aap_lv2_plugin_new,
                                         aaplv2bridge::aap_lv2_plugin_delete};
}

} // extern "C"
