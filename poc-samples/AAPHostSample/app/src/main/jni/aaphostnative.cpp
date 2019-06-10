
#include <math.h>
#include <vector>
#include <lilv/lilv.h>
#include <jni.h>

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
/*
inline bool IS_AUDIO_PORT (LilvPlugin* plugin, LilvPort* port) { return lilv_port_is_a (plugin, port, audio_port_uri_node); }
inline bool IS_INPUT_PORT (LilvPlugin* plugin, LilvPort* port) { return lilv_port_is_a (plugin, port, input_port_uri_node); }
inline bool IS_OUTPUT_PORT (LilvPlugin* plugin, LilvPort* port) { return lilv_port_is_a (plugin, port, output_port_uri_node); }
inline bool IS_CONTROL_PORT (LilvPlugin* plugin, LilvPort* port) { return lilv_port_is_a (plugin, port, control_port_uri_node); }
inline bool IS_AUDIO_IN (LilvPlugin* plugin, LilvPort* port) { return IS_AUDIO_PORT (plugin, port) && IS_INPUT_PORT (plugin, port); }
inline bool IS_AUDIO_OUT (LilvPlugin* plugin, LilvPort* port) { return IS_AUDIO_PORT (plugin, port) && IS_OUTPUT_PORT (plugin, port); }
inline bool IS_CONTROL_IN (LilvPlugin* plugin, LilvPort* port) { return IS_CONTROL_PORT (plugin, port) && IS_INPUT_PORT (plugin, port); }
*/

LV2_URID urid_map_func (LV2_URID_Map_Handle handle, const char *uri)
{
    printf ("-- map %s\n", uri);
    return 0;
}

typedef struct {
    const LilvPlugin *plugin;
    LilvInstance *instance;
    float *buffer;
} InstanceUse;


int runHost (const char** pluginUris, int numPluginUris)
{
    auto world = lilv_world_new ();
    lilv_world_load_all (world);

    audio_port_uri_node = lilv_new_uri (world, LV2_CORE__AudioPort);
    control_port_uri_node = lilv_new_uri (world, LV2_CORE__ControlPort);
    input_port_uri_node = lilv_new_uri (world, LV2_CORE__InputPort);
    output_port_uri_node = lilv_new_uri (world, LV2_CORE__OutputPort);

    auto allPlugins = lilv_world_get_all_plugins (world);
    LV2_Feature* features [3];
    auto mapData = LV2_URID_Map { NULL, urid_map_func };
    LV2_Feature uridFeature = { LV2_URID_MAP_URI, &mapData };
    //features [0] = NULL;
    features [0] = &uridFeature;
    features [1] = NULL;

    int buffer_size = 10000;

    std::vector<InstanceUse*> instances;

    for (int i = 1; i < numPluginUris; i++) {
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

    float audioIn [buffer_size];
    float controlIn [buffer_size];
    float dummyBuffer [buffer_size];

    float *currentAudioIn, *currentAudioOut;

    /* connect ports */
    puts ("Establishing port connections...");

    currentAudioIn = audioIn;

    for (int i = 0; i < instances.size(); i++) {
        auto iu = instances [i];
        auto plugin = iu->plugin;
        auto instance = iu->instance;
        printf (" Plugin '%s': connecting ports...\n", lilv_node_as_string (lilv_plugin_get_name (plugin)));
        for (uint p = 0; p < lilv_plugin_get_num_ports (plugin); p++) {
            const LilvPort *port = lilv_plugin_get_port_by_index (plugin, p);
            if (IS_AUDIO_IN (plugin, port)) {
                printf ("   port '%i': AudioIn\n", p);
                lilv_instance_connect_port (instance, p, currentAudioIn);
            } else if (IS_AUDIO_OUT (plugin, port)) {
                printf ("   port '%i': AudioOut\n", p);
                currentAudioOut = iu->buffer;
                lilv_instance_connect_port (instance, p, currentAudioOut);
            } else if (IS_CONTROL_IN (plugin, port)) {
                printf ("   port '%i': ControlIn\n", p);
                lilv_instance_connect_port (instance, p, controlIn);
            } else if (IS_CONTROL_OUT (plugin, port)) {
                printf ("   port '%i': ControlOut - connecting to dummy\n", p);
                lilv_instance_connect_port (instance, p, dummyBuffer);
            } else if (IS_CV_PORT (plugin, port)) {
                printf ("   port '%i': CV - connecting to dummy\n", p);
                lilv_instance_connect_port (instance, p, dummyBuffer);
            } else {
                printf ("   port '%i': UNKNOWN: %s\n", p, lilv_node_as_string (lilv_port_get_name(plugin, port)));
                lilv_instance_connect_port (instance, p, dummyBuffer);
            }
        }
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
        printf ("%f ", audioIn [i]);
    puts ("");
    puts ("Outputs: ");
    for (int i = 0; i < 200; i++)
        printf ("%f ", currentAudioOut [i]);
    puts ("");

    puts ("Audio processing done. Freeing everything...");
    for (int i = 0; i < instances.size(); i++) {
        auto iu = instances [i];
        free (iu->buffer);
        lilv_instance_free (iu->instance);
        delete iu;
    }

    lilv_world_free (world);

    puts ("Everything successfully done.");

    return 0;
}

extern "C" {

jint Java_org_androidaudiopluginframework_hosting_AAPLV2Host_runHost(JNIEnv *env, jclass cls, jobjectArray plugins)
{
    jsize size = env->GetArrayLength(plugins);
    jboolean isCopy = JNI_TRUE;
    const char *pluginUris[size];
    for (int i = 0; i < size; i++) {
        auto strObj = (jstring) env->GetObjectArrayElement(plugins, i);
        pluginUris[i] = env->GetStringUTFChars(strObj, &isCopy);
    }
    return runHost(pluginUris, size);
}
jint Java_org_androidaudiopluginframework_hosting_AAPLV2Host_runHostOne(JNIEnv *env, jclass cls, jstring plugin)
{
    jsize size = 1;
    jboolean isCopy = JNI_TRUE;
    const char *pluginUris[size];
    for (int i = 0; i < size; i++) {
        auto strObj = plugin;
        pluginUris[i] = env->GetStringUTFChars(strObj, &isCopy);
    }
    return runHost(pluginUris, size);
}

}