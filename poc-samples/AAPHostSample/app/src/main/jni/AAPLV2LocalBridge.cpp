#include <jni.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <android/log.h>
#include <unistd.h>
#include <dlfcn.h>
#include <cmath>
#include <cstring>
#include <vector>

#include "../../../../../../include/android-audio-plugin-host.hpp"

// FIXME: sort out final library header structures.
namespace aap {
    extern aap::PluginInformation *pluginInformation_fromJava(JNIEnv *env, jobject pluginInformation);
    extern const char *strdup_fromJava(JNIEnv *env, jstring s);
}

namespace aaplv2 {

const char *lv2Path;

// In this implementation we have fixed buffers and everything is simplified
typedef struct {
    aap::PluginInstance *plugin;
    AndroidAudioPluginBuffer *plugin_buffer;
} AAPInstanceUse;

int runHostAAP(int sampleRate, aap::PluginInformation **pluginInfos,
               const char **pluginIDs, int numPluginIDs, void *wav, int wavLength,
               void *outWav) {
    setenv("LV2_PATH", lv2Path, true);
    auto host = new aap::PluginHost(pluginInfos);

    int buffer_size = wavLength;
    int float_count = buffer_size / sizeof(float);

    std::vector<AAPInstanceUse *> instances;

    /* instantiate plugins and connect ports */

    float *audioIn = (float *) calloc(buffer_size, 1);
    float *midiIn = (float *) calloc(buffer_size, 1);
    float *controlIn = (float *) calloc(buffer_size, 1);
    float *dummyBuffer = (float *) calloc(buffer_size, 1);

    float *currentAudioIn = audioIn, *currentAudioOut = NULL, *currentMidiIn = midiIn, *currentMidiOut = NULL;

    // FIXME: pluginIDs should be enough (but iteration by it crashes so far)
    for (int i = 0; i < numPluginIDs; i++) {
        auto instance = host->instantiatePlugin(pluginIDs[i]);
        if (instance == NULL) {
            // FIXME: the entire code needs review to eliminate those printf/puts/stdout/stderr uses.
            printf("plugin %s failed to instantiate. Skipping.\n", pluginIDs[i]);
            continue;
        }
        AAPInstanceUse *iu = new AAPInstanceUse();
        auto desc = instance->getPluginDescriptor();
        iu->plugin = instance;
        iu->plugin_buffer = new AndroidAudioPluginBuffer();
        iu->plugin_buffer->num_frames = buffer_size / sizeof(float);
        int nPorts = desc->getNumPorts();
        iu->plugin_buffer->buffers = (void **) calloc(nPorts + 1, sizeof(void *));
        for (int p = 0; p < nPorts; p++) {
            auto port = desc->getPort(p);
            if (port->getPortDirection() == aap::AAP_PORT_DIRECTION_INPUT &&
                port->getContentType() == aap::AAP_CONTENT_TYPE_AUDIO)
                iu->plugin_buffer->buffers[p] = currentAudioIn;
            else if (port->getPortDirection() == aap::AAP_PORT_DIRECTION_OUTPUT &&
                     port->getContentType() == aap::AAP_CONTENT_TYPE_AUDIO)
                iu->plugin_buffer->buffers[p] = currentAudioOut = (float *) calloc(buffer_size,
                                                                                   1);
            else if (port->getPortDirection() == aap::AAP_PORT_DIRECTION_INPUT &&
                     port->getContentType() == aap::AAP_CONTENT_TYPE_MIDI)
                iu->plugin_buffer->buffers[p] = currentMidiIn;
            else if (port->getPortDirection() == aap::AAP_PORT_DIRECTION_OUTPUT &&
                     port->getContentType() == aap::AAP_CONTENT_TYPE_MIDI)
                iu->plugin_buffer->buffers[p] = currentMidiOut = (float *) calloc(buffer_size,
                                                                                  1);
            else if (port->getPortDirection() == aap::AAP_PORT_DIRECTION_INPUT)
                iu->plugin_buffer->buffers[p] = controlIn;
            else
                iu->plugin_buffer->buffers[p] = dummyBuffer;
        }
        instances.push_back(iu);
        if (currentAudioOut)
            currentAudioIn = currentAudioOut;
        if (currentMidiOut)
            currentMidiIn = currentMidiOut;
    }

    assert(instances.size() > 0);

    // prepare connections
    for (int i = 0; i < instances.size(); i++) {
        auto iu = instances[i];
        auto plugin = iu->plugin;
        plugin->prepare(sampleRate, false, iu->plugin_buffer);
    }

    // prepare inputs
    memcpy(audioIn, wav, buffer_size);
    for (int i = 0; i < float_count; i++)
        controlIn[i] = 0.5;

    // activate, run, deactivate
    for (int i = 0; i < instances.size(); i++) {
        auto instance = instances[i];
        instance->plugin->activate();
    }
    for (int i = 0; i < instances.size(); i++) {
        auto instance = instances[i];
        instance->plugin->process(instance->plugin_buffer, 0);
    }
    for (int i = 0; i < instances.size(); i++) {
        auto instance = instances[i];
        instance->plugin->deactivate();
    }

    memcpy(outWav, currentAudioOut, buffer_size);

    for (int i = 0; i < instances.size(); i++) {
        auto iu = instances[i];
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

typedef void (*set_io_context_func)(void *);

set_io_context_func libserd_set_context = NULL;
set_io_context_func liblilv_set_context = NULL;

void ensureDLEach(const char *libname, set_io_context_func &context) {
    if (context == NULL) {
        auto lib = dlopen(libname, RTLD_NOW);
        assert (lib != NULL);
        context = (set_io_context_func) dlsym(lib, "abstract_set_io_context");
        assert (context != NULL);
    }
}

void ensureDLLoaded() {
    ensureDLEach("libserd-0.so", libserd_set_context);
    ensureDLEach("liblilv-0.so", liblilv_set_context);
}

void set_io_context(AAssetManager *am) {
    ensureDLLoaded();
    libserd_set_context(am);
    liblilv_set_context(am);
}

void cleanup ()
{
    if (lv2Path)
        free((void*) lv2Path);
    lv2Path = NULL;
    set_io_context(NULL);
}

} // namesoace aaplv2

extern "C" {

void Java_org_androidaudiopluginframework_hosting_AAPLV2LocalHost_initialize(JNIEnv *env, jclass cls, jobject lv2PathString, jobject assets)
{
    aaplv2::set_io_context(AAssetManager_fromJava(env, assets));

    jboolean isCopy = JNI_TRUE;
    auto s = env->GetStringUTFChars((jstring) lv2PathString, &isCopy);
    s = isCopy ? s : strdup(s);
    aaplv2::lv2Path = s;
}

void Java_org_androidaudiopluginframework_hosting_AAPLV2LocalHost_cleanup(JNIEnv *env, jclass cls)
{
    aaplv2::cleanup();
    free((void*) aaplv2::lv2Path);
}

jint Java_org_androidaudiopluginframework_hosting_AAPLV2LocalHost_runHostAAP(JNIEnv *env, jclass cls, jobjectArray jPluginInfos, jobjectArray jPlugins, jint sampleRate, jbyteArray wav, jbyteArray outWav)
{
    jsize infoSize = env->GetArrayLength(jPluginInfos);
    aap::PluginInformation *pluginInfos[infoSize];
    for (int i = 0; i < infoSize; i++) {
        auto jPluginInfo = (jobject) env->GetObjectArrayElement(jPluginInfos, i);
        pluginInfos[i] = aap::pluginInformation_fromJava(env, jPluginInfo);
    }

    jsize size = env->GetArrayLength(jPlugins);
    const char *pluginIDs[size];
    for (int i = 0; i < size; i++) {
        auto strUriObj = (jstring) env->GetObjectArrayElement(jPlugins, i);
        pluginIDs[i] = aap::strdup_fromJava(env, strUriObj);
    }

    int wavLength = env->GetArrayLength(wav);
    void* wavBytes = calloc(wavLength, 1);
    env->GetByteArrayRegion(wav, 0, wavLength, (jbyte*) wavBytes);
    void* outWavBytes = calloc(wavLength, 1);

    int ret = aaplv2::runHostAAP(sampleRate, pluginInfos, pluginIDs, size, wavBytes, wavLength, outWavBytes);

    env->SetByteArrayRegion(outWav, 0, wavLength, (jbyte*) outWavBytes);

    for(int i = 0; i < infoSize; i++)
        delete pluginInfos[i];

    for(int i = 0; i < size; i++)
        free((char*) pluginIDs[i]);
    free(wavBytes);
    free(outWavBytes);
    return ret;
}

}