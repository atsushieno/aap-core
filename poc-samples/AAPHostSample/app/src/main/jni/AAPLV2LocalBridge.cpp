#include <jni.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
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
#include "../../../../../../include/android-audio-plugin-host.hpp"

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


int runHostLilv(const char* lv2Path, const char** pluginUris, int numPluginUris, void* wav, int wavLength, void* outWav)
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

    int buffer_size = wavLength;
    int float_count = buffer_size / sizeof(float);

    std::vector<InstanceUse*> instances;

    for (int i = 0; i < numPluginUris; i++) {
        auto pluginUriNode = lilv_new_uri (world, pluginUris [i]);
        const LilvPlugin *plugin = lilv_plugins_get_by_uri (allPlugins, pluginUriNode);
        if (plugin == NULL)
            printf ("Plugin %s could not be loaded.\n", pluginUris [i]);
        else {
            LilvInstance *instance = lilv_plugin_instantiate (plugin, global_sample_rate, features);
            if (instance == NULL) {
                printf ("plugin %s failed to instantiate. Skipping.\n", pluginUris[i]);
            }
            else {
                InstanceUse *iu = new InstanceUse ();
                iu->plugin = plugin;
                iu->instance = instance;
                iu->buffer = (float*) calloc (buffer_size, 1);
                instances.push_back (iu);
            }
        }
    }

    if (instances.size() == 0) {
        aputs ("No plugin instantiated. Quit...");
        return -1;
    }

    float *audioIn = (float*) calloc(buffer_size, 1);
    float *controlIn = (float*) calloc(buffer_size, 1);
    float *dummyBuffer = (float*) calloc(buffer_size, 1);

    float *currentAudioIn = NULL, *currentAudioOut = NULL;

    /* connect ports */

    currentAudioIn = audioIn;

    for (int i = 0; i < instances.size(); i++) {
        auto iu = instances [i];
        auto plugin = iu->plugin;
        auto instance = iu->instance;
        aprintf (" Plugin '%s': connecting ports...\n", lilv_node_as_string (lilv_plugin_get_name (plugin)));
        int numPorts = lilv_plugin_get_num_ports (plugin);
        for (uint p = 0; p < numPorts; p++) {
            const LilvPort *port = lilv_plugin_get_port_by_index (plugin, p);
            if (IS_AUDIO_IN (plugin, port)) {
                lilv_instance_connect_port (instance, p, currentAudioIn);
            } else if (IS_AUDIO_OUT (plugin, port)) {
                currentAudioOut = iu->buffer;
                lilv_instance_connect_port (instance, p, currentAudioOut);
            } else if (IS_CONTROL_IN (plugin, port)) {
                lilv_instance_connect_port (instance, p, controlIn);
            } else if (IS_CONTROL_OUT (plugin, port)) {
                aprintf ("   port '%i': ControlOut - connecting to dummy\n", p);
                lilv_instance_connect_port (instance, p, dummyBuffer);
            } else if (IS_CV_PORT (plugin, port)) {
                aprintf ("   port '%i': CV - connecting to dummy\n", p);
                lilv_instance_connect_port (instance, p, dummyBuffer);
            } else {
                aprintf ("   port '%i': UNKNOWN: %s\n", p, lilv_node_as_string (lilv_port_get_name(plugin, port)));
                lilv_instance_connect_port (instance, p, dummyBuffer);
            }
        }
        if (currentAudioOut)
            currentAudioIn = currentAudioOut;
    }
    aputs ("Port connections established. Start processing audio...");

    memcpy(audioIn, wav, buffer_size);
    for (int i = 0; i < float_count; i++)
        controlIn [i] = 0.5;

    for (int i = 0; i < instances.size(); i++) {
        auto instance = instances [i];
        lilv_instance_activate (instance->instance);
    }
    for (int i = 0; i < instances.size(); i++) {
        auto instance = instances [i];
        lilv_instance_run (instance->instance, float_count);
    }
    for (int i = 0; i < instances.size(); i++) {
        auto instance = instances [i];
        lilv_instance_deactivate (instance->instance);
    }

    memcpy(outWav, currentAudioOut, buffer_size);

    for (int i = 0; i < instances.size(); i++) {
        auto iu = instances [i];
        free (iu->buffer);
        lilv_instance_free (iu->instance);
        delete iu;
    }

    lilv_world_free (world);

    free(audioIn);
    free(controlIn);
    free(dummyBuffer);

    return 0;
}

// In this implementation we have fixed buffers and everything is simplified
typedef struct {
    aap::PluginInstance *plugin;
    AndroidAudioPluginBuffer *plugin_buffer;
} AAPInstanceUse;

int runHostAAP(int sampleRate, const char** pluginIDs, int numPluginIDs, void* wav, int wavLength, void* outWav)
{
    aap::PluginInformation** pluginInfos; // FIXME: convert from pluginIDs
    auto host = new aap::PluginHost(pluginInfos);

    int buffer_size = wavLength;
    int float_count = buffer_size / sizeof(float);

    std::vector<AAPInstanceUse*> instances;

    /* instantiate plugins and connect ports */

    float *audioIn = (float*) calloc(buffer_size, 1);
    float *midiIn = (float*) calloc(buffer_size, 1);
    float *dummyBuffer = (float*) calloc(buffer_size, 1);

    float *currentAudioIn = audioIn, *currentAudioOut = NULL, *currentMidiIn = midiIn, *currentMidiOut = NULL;

    for (int i = 0; pluginIDs + i; i++) {
        auto instance = host->instantiatePlugin(pluginIDs[i]);
        if (instance == NULL) {
            printf("plugin %s failed to instantiate. Skipping.\n", pluginIDs[i]);
            continue;
        }
        AAPInstanceUse *iu = new AAPInstanceUse();
        auto desc = instance->getPluginDescriptor();
        iu->plugin = instance;
        iu->plugin_buffer = new AndroidAudioPluginBuffer();
        iu->plugin_buffer->num_frames = buffer_size / sizeof(float);
        int nPorts = desc->getNumPorts();
        iu->plugin_buffer->buffers = (void**) calloc(nPorts + 1, sizeof(void*));
        for (int p = 0; p < nPorts; p++) {
            auto port = desc->getPort(p);
            if (port->getPortDirection() == aap::AAP_PORT_DIRECTION_INPUT && port->getContentType() == aap::AAP_CONTENT_TYPE_AUDIO)
                iu->plugin_buffer->buffers[p] = currentAudioIn;
            else if (port->getPortDirection() == aap::AAP_PORT_DIRECTION_OUTPUT && port->getContentType() == aap::AAP_CONTENT_TYPE_AUDIO)
                iu->plugin_buffer->buffers[p] = currentAudioOut = (float *) calloc(buffer_size, 1);
            else if (port->getPortDirection() == aap::AAP_PORT_DIRECTION_INPUT && port->getContentType() == aap::AAP_CONTENT_TYPE_MIDI)
                iu->plugin_buffer->buffers[p] = currentMidiIn;
            else if (port->getPortDirection() == aap::AAP_PORT_DIRECTION_OUTPUT && port->getContentType() == aap::AAP_CONTENT_TYPE_MIDI)
                iu->plugin_buffer->buffers[p] = currentMidiOut = (float *) calloc(buffer_size, 1);
        }
        instances.push_back(iu);
        if (currentAudioOut)
            currentAudioIn = currentAudioOut;
        if (currentMidiOut)
            currentMidiIn = currentMidiOut;
    }

    if (instances.size() == 0) {
        aputs ("No plugin instantiated. Quit...");
        return -1;
    }

    // prepare connections
    for (int i = 0; i < instances.size(); i++) {
        auto iu = instances[i];
        auto plugin = iu->plugin;
        plugin->prepareToPlay(sampleRate, false);
    }

    // prepare inputs
    memcpy(audioIn, wav, buffer_size);
    for (int i = 0; i < float_count; i++)
        midiIn [i] = 0.5;

    // activate, run, deactivate
    for (int i = 0; i < instances.size(); i++) {
        auto instance = instances [i];
        instance->plugin->activate();
    }
    for (int i = 0; i < instances.size(); i++) {
        auto instance = instances [i];
        instance->plugin->process(instance->plugin_buffer, 0);
    }
    for (int i = 0; i < instances.size(); i++) {
        auto instance = instances [i];
        instance->plugin->deactivate();
    }

    memcpy(outWav, currentAudioOut, buffer_size);

    for (int i = 0; i < instances.size(); i++) {
        auto iu = instances [i];
        for (int p = 0; iu->plugin_buffer->buffers[p]; p++)
            free(iu->plugin_buffer->buffers[p]);
        free(iu->plugin_buffer->buffers);
        delete iu->plugin_buffer;
        delete iu->plugin;
        delete iu;
    }

    delete host;

    free(audioIn);
    free(midiIn);
    free(dummyBuffer);

    return 0;
}


extern "C" {

typedef void (*set_io_context_func) (void*);
set_io_context_func libserd_set_context = NULL;
set_io_context_func liblilv_set_context = NULL;

void ensureDLEach(const char* libname, set_io_context_func &context)
{
    if (context == NULL) {
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

char *lv2Path;

void Java_org_androidaudiopluginframework_hosting_AAPLV2LocalHost_initialize(JNIEnv *env, jclass cls, jobjectArray jPluginPaths, jobject assets)
{
    set_io_context(AAssetManager_fromJava(env, assets));

    jboolean isCopy = JNI_TRUE;
    int lv2PathLen = 0;
    jsize pathsSize = env->GetArrayLength(jPluginPaths);
    const char *pluginPaths[pathsSize];
    for (int i = 0; i < pathsSize; i++) {
        auto strPathObj = (jstring) env->GetObjectArrayElement(jPluginPaths, i);
        pluginPaths[i] = env->GetStringUTFChars(strPathObj, &isCopy);
        if (!isCopy)
            pluginPaths[i] = strdup(pluginPaths[i]);
        lv2PathLen += strlen(pluginPaths[i] + 1);
    }
    lv2Path = (char*) calloc(lv2PathLen, 1);
    lv2PathLen = 0;
    for (int i = 0; i < pathsSize; i++) {
        strcpy(lv2Path + lv2PathLen, pluginPaths[i]);
        lv2PathLen += strlen(pluginPaths[i]);
        lv2Path[lv2PathLen++] = ':';
    }
    lv2Path[lv2PathLen] = NULL;
    for(int i = 0; i < pathsSize; i++)
        free((char*) pluginPaths[i]);
}

void Java_org_androidaudiopluginframework_hosting_AAPLV2LocalHost_cleanup(JNIEnv *env, jclass cls)
{
    if (lv2Path)
        free(lv2Path);
    lv2Path = NULL;
    set_io_context(NULL);
}

jint Java_org_androidaudiopluginframework_hosting_AAPLV2LocalHost_runHostLilv(JNIEnv *env, jclass cls, jobjectArray jPlugins, jbyteArray wav, jbyteArray outWav)
{
    jboolean isCopy = JNI_TRUE;

    jsize size = env->GetArrayLength(jPlugins);
    const char *pluginUris[size];
    for (int i = 0; i < size; i++) {
        auto strUriObj = (jstring) env->GetObjectArrayElement(jPlugins, i);
        pluginUris[i] = env->GetStringUTFChars(strUriObj, &isCopy);
        if (!isCopy)
            pluginUris[i] = strdup(pluginUris[i]);
    }

    int wavLength = env->GetArrayLength(wav);
    void* wavBytes = calloc(wavLength, 1);
    env->GetByteArrayRegion(wav, 0, wavLength, (jbyte*) wavBytes);
    void* outWavBytes = calloc(wavLength, 1);

    int ret = runHostLilv(lv2Path, pluginUris, size, wavBytes, wavLength, outWavBytes);

    env->SetByteArrayRegion(outWav, 0, wavLength, (jbyte*) outWavBytes);

    set_io_context(NULL);

    for(int i = 0; i < size; i++)
        free((char*) pluginUris[i]);
    free(wavBytes);
    free(outWavBytes);
    return ret;
}

jint Java_org_androidaudiopluginframework_hosting_AAPLV2LocalHost_runHostAAP(JNIEnv *env, jclass cls, jobjectArray jPlugins, jint sampleRate, jbyteArray wav, jbyteArray outWav)
{
    jboolean isCopy = JNI_TRUE;

    jsize size = env->GetArrayLength(jPlugins);
    const char *pluginIDs[size];
    for (int i = 0; i < size; i++) {
        auto strUriObj = (jstring) env->GetObjectArrayElement(jPlugins, i);
        pluginIDs[i] = env->GetStringUTFChars(strUriObj, &isCopy);
        if (!isCopy)
            pluginIDs[i] = strdup(pluginIDs[i]);
    }

    int wavLength = env->GetArrayLength(wav);
    void* wavBytes = calloc(wavLength, 1);
    env->GetByteArrayRegion(wav, 0, wavLength, (jbyte*) wavBytes);
    void* outWavBytes = calloc(wavLength, 1);

    int ret = runHostAAP(sampleRate, pluginIDs, size, wavBytes, wavLength, outWavBytes);

    env->SetByteArrayRegion(outWav, 0, wavLength, (jbyte*) outWavBytes);

    set_io_context(NULL);

    for(int i = 0; i < size; i++)
        free((char*) pluginIDs[i]);
    free(wavBytes);
    free(outWavBytes);
    return ret;
}

}