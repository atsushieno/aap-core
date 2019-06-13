#include <jni.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <android/log.h>
#include <unistd.h>

#include <math.h>
#include <string.h>
#include <vector>
#include <map>
#include <lilv/lilv.h>
#include <../lib/lv2/atom.lv2/atom.h>
#include <../lib/lv2/log.lv2/log.h>
#include <../lib/lv2/buf-size.lv2/buf-size.h>
#include <dlfcn.h>

#define PI 3.14159265

const int global_sample_rate = 44100;


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
    const LilvPlugin *plugin;
    LilvInstance *instance;
    float *buffer;
} InstanceUse;


int runHost (const char* lv2Path, const char** pluginUris, int numPluginUris)
{
    auto world = lilv_world_new ();
    auto lv2_path_node = lilv_new_string(world, lv2Path);
    lilv_world_set_option(world, LILV_OPTION_LV2_PATH, lv2_path_node);
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
    //features [0] = NULL;
    features [0] = &uridFeature;
    features [1] = &logFeature;
    features [2] = &bufSizeFeature;
    features [3] = &atomFeature;
    features [4] = NULL;

    int buffer_size = 10000;

    std::vector<InstanceUse*> instances;

    for (int i = 0; i < numPluginUris; i++) {
        auto pluginUriNode = lilv_new_uri (world, pluginUris [i]);
        const LilvPlugin *plugin = lilv_plugins_get_by_uri (allPlugins, pluginUriNode);
        if (plugin == NULL)
            printf ("Plugin %s could not be loaded.\n", pluginUris [i]);
        else {
            printf ("Loading Plugin %s ...\n", pluginUris [i]);

            LilvInstance *instance = lilv_plugin_instantiate (plugin, global_sample_rate, features);
            if (instance == NULL) {
                printf (" skipped\n");
            }
            else {
                printf (" loaded\n");
                InstanceUse *iu = new InstanceUse ();
                iu->plugin = plugin;
                iu->instance = instance;
                iu->buffer = (float*) malloc (sizeof (float) * buffer_size);
                instances.push_back (iu);
            }
        }
    }

    if (instances.size() == 0) {
        aputs ("No plugin instantiated. Quit...");
        return -1;
    }

    float audioIn [buffer_size];
    float controlIn [buffer_size];
    float dummyBuffer [buffer_size];

    float *currentAudioIn = NULL, *currentAudioOut = NULL;

    /* connect ports */
    puts ("Establishing port connections...");

    currentAudioIn = audioIn;

    for (int i = 0; i < instances.size(); i++) {
        auto iu = instances [i];
        auto plugin = iu->plugin;
        auto instance = iu->instance;
        aprintf (" Plugin '%s': connecting ports...\n", lilv_node_as_string (lilv_plugin_get_name (plugin)));
        for (uint p = 0; p < lilv_plugin_get_num_ports (plugin); p++) {
            const LilvPort *port = lilv_plugin_get_port_by_index (plugin, p);
            if (IS_AUDIO_IN (plugin, port)) {
                aprintf ("   port '%i': AudioIn\n", p);
                lilv_instance_connect_port (instance, p, currentAudioIn);
            } else if (IS_AUDIO_OUT (plugin, port)) {
                aprintf ("   port '%i': AudioOut\n", p);
                currentAudioOut = iu->buffer;
                lilv_instance_connect_port (instance, p, currentAudioOut);
            } else if (IS_CONTROL_IN (plugin, port)) {
                aprintf ("   port '%i': ControlIn\n", p);
                lilv_instance_connect_port (instance, p, controlIn);
            } else if (IS_CONTROL_OUT (plugin, port)) {
                aprintf ("   port '%i': ControlOut - connecting to dummy\n", p);
                lilv_instance_connect_port (instance, p, dummyBuffer);
            } else if (IS_CV_PORT (plugin, port)) {
                aprintf ("   port '%i': CV - connecting to dummy\n", p);
                lilv_instance_connect_port (instance, p, dummyBuffer);
            } else {
                printf ("   port '%i': UNKNOWN: %s\n", p, lilv_node_as_string (lilv_port_get_name(plugin, port)));
                lilv_instance_connect_port (instance, p, dummyBuffer);
            }
        }
        if (currentAudioOut)
            currentAudioIn = currentAudioOut;
    }
    puts ("Port connections established. Start processing audio...");

    for (int i = 0; i < buffer_size; i++)
        audioIn [i] = (float) sin (i / 50.0 * PI);
    for (int i = 0; i < buffer_size; i++)
        controlIn [i] = 0.5;

    for (int i = 0; i < instances.size(); i++) {
        auto instance = instances [i];
        lilv_instance_activate (instance->instance);
    }
    for (int i = 0; i < instances.size(); i++) {
        auto instance = instances [i];
        lilv_instance_run (instance->instance, buffer_size);
    }
    for (int i = 0; i < instances.size(); i++) {
        auto instance = instances [i];
        lilv_instance_deactivate (instance->instance);
    }
    puts ("Audio processing done.");


    puts ("Inputs: ");
    for (int i = 0; i < 200; i++)
        aprintf ("%f ", audioIn [i]);
    puts ("");
    if (currentAudioOut) {
        aputs ("Outputs: ");
        for (int i = 0; i < 200; i++)
            printf ("%f ", currentAudioOut [i]);
        aputs ("");
    }

    aputs ("Audio processing done. Freeing everything...");
    for (int i = 0; i < instances.size(); i++) {
        auto iu = instances [i];
        free (iu->buffer);
        lilv_instance_free (iu->instance);
        delete iu;
    }

    lilv_world_free (world);

    aputs ("Everything successfully done.");

    return 0;
}

extern "C" {

typedef void (*set_io_context_func) (void*);
set_io_context_func libserd_set_context = NULL;
set_io_context_func liblilv_set_context = NULL;

void ensureDLEach(const char* libname, set_io_context_func &context)
{
    if (liblilv_set_context == NULL) {
        auto lib = dlopen(libname, RTLD_NOW);
        assert (lib != NULL);
        context = (set_io_context_func) dlsym(lib, "abstract_set_io_context");
        assert (context != NULL);
    }
}

void ensureDLLoaded()
{
    ensureDLEach("libserd-0.so", libserd_set_context);
    ensureDLEach("liblilv-0.so", liblilv_set_context);
}

void set_io_context(AAssetManager *am)
{
    ensureDLLoaded();
    libserd_set_context(am);
    liblilv_set_context(am);
}

jint Java_org_androidaudiopluginframework_hosting_AAPLV2Host_runHost(JNIEnv *env, jclass cls, jobjectArray jPluginPaths, jobject assets, jobjectArray jPlugins)
{
    set_io_context(AAssetManager_fromJava(env, assets));

    jboolean isCopy = JNI_TRUE;

    int lv2PathLen = 0;
    jsize pathsSize = env->GetArrayLength(jPluginPaths);
    const char *pluginPaths[pathsSize];
    for (int i = 0; i < pathsSize; i++) {
        auto strPathObj = (jstring) env->GetObjectArrayElement(jPluginPaths, i);
        // FIXME: leaky (wrt isCopy)
        pluginPaths[i] = env->GetStringUTFChars(strPathObj, &isCopy);
        lv2PathLen += strlen(pluginPaths[i] + 1);
    }
    char lv2Path[lv2PathLen];
    lv2PathLen = 0;
    for (int i = 0; i < pathsSize; i++) {
        strcpy(lv2Path + lv2PathLen, pluginPaths[i]);
        lv2PathLen += strlen(pluginPaths[i]);
        lv2Path[lv2PathLen++] = ':';
    }
    lv2Path[lv2PathLen] = NULL;
    __android_log_print( ANDROID_LOG_INFO, "AAAAAAAAAAAAA", "%s", lv2Path);

    jsize size = env->GetArrayLength(jPlugins);
    const char *pluginUris[size];
    for (int i = 0; i < size; i++) {
        auto strUriObj = (jstring) env->GetObjectArrayElement(jPlugins, i);
        pluginUris[i] = env->GetStringUTFChars(strUriObj, &isCopy);
    }
    int ret = runHost(lv2Path, pluginUris, size);

    set_io_context(NULL);

    return ret;
}

}