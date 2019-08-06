#include <jni.h>
#include <cstdint>
#include <vector>
#include <map>
#include <android/log.h>
#include <lilv/lilv.h>
#include <../lib/lv2/atom.lv2/atom.h>
#include <../lib/lv2/log.lv2/log.h>
#include <../lib/lv2/buf-size.lv2/buf-size.h>
#include "aap/android-audio-plugin-host.hpp"

namespace lv2direct {

    LilvNode *audio_port_uri_node, *control_port_uri_node, *cv_port_uri_node, *input_port_uri_node, *output_port_uri_node;

    typedef struct {
        const LilvPlugin *plugin;
        LilvInstance *instance;
        float *buffer;
    } InstanceUse;

    typedef struct {
        bool operator()(char *p1, char *p2) { return strcmp(p1, p2) == 0; }
    } uricomp;

    LV2_URID urid_map_func(LV2_URID_Map_Handle handle, const char *uri) {
        printf("-- map %s", uri);
        auto map = static_cast<std::map<const char *, LV2_URID> *> (handle);
        auto it = map->find(uri);
        if (it == map->end())
            map->emplace(uri, map->size() + 1);
        auto ret = map->find(uri)->second;
        printf(" ... as %d\n", ret);
        return ret;
    }

    int avprintf(const char *fmt, va_list ap) {
        return __android_log_print(ANDROID_LOG_INFO, "AAPHostNative", fmt, ap);
    }

    int aprintf(const char *fmt, ...) {
        va_list ap;
        va_start (ap, fmt);
        return avprintf(fmt, ap);
    }

    void aputs(const char *s) {
        __android_log_print(ANDROID_LOG_INFO, "AAPHostNative", "%s", s);
    }

    int log_vprintf(LV2_Log_Handle handle, LV2_URID type, const char *fmt, va_list ap) {
        int ret = aprintf("LV2 LOG (%d): ", type);
        ret += aprintf(fmt, ap);
        return ret;
    }

    int log_printf(LV2_Log_Handle handle, LV2_URID type, const char *fmt, ...) {
        va_list ap;
        va_start (ap, fmt);
        return log_vprintf(handle, type, fmt, ap);
    }

#define PORTCHECKER_SINGLE(_name_, _type_) inline bool _name_ (const LilvPlugin* plugin, const LilvPort* port) { return lilv_port_is_a (plugin, port, _type_); }
#define PORTCHECKER_AND(_name_, _cond1_, _cond2_) inline bool _name_ (const LilvPlugin* plugin, const LilvPort* port) { return _cond1_ (plugin, port) && _cond2_ (plugin, port); }

    PORTCHECKER_SINGLE (IS_AUDIO_PORT, audio_port_uri_node)
    PORTCHECKER_SINGLE (IS_INPUT_PORT, input_port_uri_node)
    PORTCHECKER_SINGLE (IS_OUTPUT_PORT, output_port_uri_node)
    PORTCHECKER_SINGLE (IS_CONTROL_PORT, control_port_uri_node)
    PORTCHECKER_SINGLE (IS_CV_PORT, cv_port_uri_node)
    PORTCHECKER_AND (IS_AUDIO_IN, IS_AUDIO_PORT, IS_INPUT_PORT)
    PORTCHECKER_AND (IS_AUDIO_OUT, IS_AUDIO_PORT, IS_OUTPUT_PORT)
    PORTCHECKER_AND (IS_CONTROL_IN, IS_CONTROL_PORT, IS_INPUT_PORT)
    PORTCHECKER_AND (IS_CONTROL_OUT, IS_CONTROL_PORT, IS_OUTPUT_PORT)

    int runHostLilv(int sampleRate, const char **pluginUris, int numPluginUris, void *wav,
                    int wavLength, void *outWav) {
        auto world = lilv_world_new();
        lilv_world_load_all(world);

        audio_port_uri_node = lilv_new_uri(world, LV2_CORE__AudioPort);
        control_port_uri_node = lilv_new_uri(world, LV2_CORE__ControlPort);
        input_port_uri_node = lilv_new_uri(world, LV2_CORE__InputPort);
        output_port_uri_node = lilv_new_uri(world, LV2_CORE__OutputPort);

        auto allPlugins = lilv_world_get_all_plugins(world);
        _LV2_Feature *features[5];
        auto urid_map = new std::map<char *, uint32_t, uricomp>();
        auto mapData = _LV2_URID_Map{urid_map, urid_map_func};
        auto logData = _LV2_Log{NULL, log_printf, log_vprintf};
        _LV2_Feature uridFeature = {LV2_URID_MAP_URI, &mapData};
        _LV2_Feature logFeature = {LV2_LOG_URI, &logData};
        _LV2_Feature bufSizeFeature = {LV2_BUF_SIZE_URI, NULL};
        _LV2_Feature atomFeature = {LV2_ATOM_URI, NULL};
        //features [0] = NULL;
        features[0] = &uridFeature;
        features[1] = &logFeature;
        features[2] = &bufSizeFeature;
        features[3] = &atomFeature;
        features[4] = NULL;

        int buffer_size = wavLength;
        int float_count = buffer_size / sizeof(float);

        std::vector < InstanceUse * > instances;

        for (int i = 0; i < numPluginUris; i++) {
            auto pluginUriNode = lilv_new_uri(world, pluginUris[i]);
            const struct LilvPluginImpl *plugin = lilv_plugins_get_by_uri(allPlugins,
                                                                          pluginUriNode);
            if (plugin == NULL)
                printf("Plugin %s could not be loaded.\n", pluginUris[i]);
            else {
                struct LilvInstanceImpl *instance = lilv_plugin_instantiate(plugin, sampleRate,
                                                                            features);
                if (instance == NULL) {
                    printf("plugin %s failed to instantiate. Skipping.\n", pluginUris[i]);
                } else {
                    InstanceUse *iu = new InstanceUse();
                    iu->plugin = plugin;
                    iu->instance = instance;
                    iu->buffer = (float *) calloc(buffer_size, 1);
                    instances.push_back(iu);
                }
            }
        }

        if (instances.size() == 0) {
            aputs("No plugin instantiated. Quit...");
            return -1;
        }

        float *audioIn = (float *) calloc(buffer_size, 1);
        float *controlIn = (float *) calloc(buffer_size, 1);
        float *dummyBuffer = (float *) calloc(buffer_size, 1);

        float *currentAudioIn = NULL, *currentAudioOut = NULL;

        /* connect ports */

        currentAudioIn = audioIn;

        for (int i = 0; i < instances.size(); i++) {
            auto iu = instances[i];
            auto plugin = iu->plugin;
            auto instance = iu->instance;
            aprintf(" Plugin '%s': connecting ports...\n",
                            lilv_node_as_string(lilv_plugin_get_name(plugin)));
            int numPorts = lilv_plugin_get_num_ports(plugin);
            for (unsigned int p = 0; p < numPorts; p++) {
                const struct LilvPortImpl *port = lilv_plugin_get_port_by_index(plugin, p);
                if (IS_AUDIO_IN(plugin, port)) {
                    lilv_instance_connect_port(instance, p, currentAudioIn);
                } else if (IS_AUDIO_OUT(plugin, port)) {
                    currentAudioOut = iu->buffer;
                    lilv_instance_connect_port(instance, p, currentAudioOut);
                } else if (IS_CONTROL_IN(plugin, port)) {
                    lilv_instance_connect_port(instance, p, controlIn);
                } else if (IS_CONTROL_OUT(plugin, port)) {
                    aprintf("   port '%i': ControlOut - connecting to dummy\n", p);
                    lilv_instance_connect_port(instance, p, dummyBuffer);
                } else if (IS_CV_PORT(plugin, port)) {
                    aprintf("   port '%i': CV - connecting to dummy\n", p);
                    lilv_instance_connect_port(instance, p, dummyBuffer);
                } else {
                    aprintf("   port '%i': UNKNOWN: %s\n", p,
                                    lilv_node_as_string(lilv_port_get_name(plugin, port)));
                    lilv_instance_connect_port(instance, p, dummyBuffer);
                }
            }
            if (currentAudioOut)
                currentAudioIn = currentAudioOut;
        }
        aputs("Port connections established. Start processing audio...");

        memcpy(audioIn, wav, buffer_size);
        for (int i = 0; i < float_count; i++)
            controlIn[i] = 0.5;

        for (int i = 0; i < instances.size(); i++) {
            auto instance = instances[i];
            lilv_instance_activate(instance->instance);
        }
        for (int i = 0; i < instances.size(); i++) {
            auto instance = instances[i];
            lilv_instance_run(instance->instance, float_count);
        }
        for (int i = 0; i < instances.size(); i++) {
            auto instance = instances[i];
            lilv_instance_deactivate(instance->instance);
        }

        memcpy(outWav, currentAudioOut, buffer_size);

        for (int i = 0; i < instances.size(); i++) {
            auto iu = instances[i];
            free(iu->buffer);
            lilv_instance_free(iu->instance);
            delete iu;
        }

        lilv_world_free(world);

        free(audioIn);
        free(controlIn);
        free(dummyBuffer);

        return 0;
    }
}

extern "C" {

jint Java_org_androidaudioplugin_aaphostsample_AAPSampleLV2Interop_runHostLilv(JNIEnv *env, jclass cls, jobjectArray jPlugins, jint sampleRate, jbyteArray wav, jbyteArray outWav)
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

    int ret = lv2direct::runHostLilv(sampleRate, pluginUris, size, wavBytes, wavLength, outWavBytes);

    env->SetByteArrayRegion(outWav, 0, wavLength, (jbyte*) outWavBytes);

    for(int i = 0; i < size; i++)
        free((char*) pluginUris[i]);
    free(wavBytes);
    free(outWavBytes);
    return ret;
}

}
