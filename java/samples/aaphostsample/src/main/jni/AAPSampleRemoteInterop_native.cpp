
#include <jni.h>
#include <android/binder_ibinder.h>
#include <android/binder_ibinder_jni.h>
#include <android/binder_interface_utils.h>
#include <android/binder_parcel.h>
#include <android/binder_parcel_utils.h>
#include <android/binder_status.h>
#include <android/binder_auto_utils.h>
#include <android/log.h>
#include "aidl/org/androidaudioplugin/BnAudioPluginInterface.h"
#include "aidl/org/androidaudioplugin/BpAudioPluginInterface.h"
#include "aap/android-audio-plugin-host.hpp"

aidl::org::androidaudioplugin::BnAudioPluginInterface *sp_binder;

extern "C" {
extern aap::PluginInformation **local_plugin_infos;
}

// FIXME: sort out final library header structures.
namespace aap {
    extern aap::PluginInformation *pluginInformation_fromJava(JNIEnv *env, jobject pluginInformation);
    extern const char *strdup_fromJava(JNIEnv *env, jstring s);
}


namespace aapremote {

int runClientAAP(aidl::org::androidaudioplugin::IAudioPluginInterface* proxy, int sampleRate, const aap::PluginInformation *pluginInfo, void *wav, int wavLength, void *outWav) {

    int buffer_size = 44100 * 2 * sizeof(float); // FIXME: (ish) get number of channels instead of '2'.
    int float_count = buffer_size / sizeof(float);

    /* instantiate plugins and connect ports */

    float *audioIn = (float *) calloc(buffer_size, 1);
    float *midiIn = (float *) calloc(buffer_size, 1);
    float *controlIn = (float *) calloc(buffer_size, 1);
    float *dummyBuffer = (float *) calloc(buffer_size, 1);

    float *currentAudioIn = audioIn, *currentAudioOut = NULL, *currentMidiIn = midiIn, *currentMidiOut = NULL;

    auto status = proxy->create(pluginInfo->getPluginID(), sampleRate);
    assert (status.isOk());

    auto desc = pluginInfo;
    auto plugin_buffer = new AndroidAudioPluginBuffer();
    plugin_buffer->num_frames = buffer_size / sizeof(float);
    int nPorts = desc->getNumPorts();
    plugin_buffer->buffers = (void **) calloc(nPorts + 1, sizeof(void *));
    for (int p = 0; p < nPorts; p++) {
        auto port = desc->getPort(p);
        if (port->getPortDirection() == aap::AAP_PORT_DIRECTION_INPUT &&
            port->getContentType() == aap::AAP_CONTENT_TYPE_AUDIO)
            plugin_buffer->buffers[p] = currentAudioIn;
        else if (port->getPortDirection() == aap::AAP_PORT_DIRECTION_OUTPUT &&
                 port->getContentType() == aap::AAP_CONTENT_TYPE_AUDIO)
            plugin_buffer->buffers[p] = currentAudioOut = (float *) calloc(buffer_size, 1);
        else if (port->getPortDirection() == aap::AAP_PORT_DIRECTION_INPUT &&
                 port->getContentType() == aap::AAP_CONTENT_TYPE_MIDI)
            plugin_buffer->buffers[p] = currentMidiIn;
        else if (port->getPortDirection() == aap::AAP_PORT_DIRECTION_OUTPUT &&
                 port->getContentType() == aap::AAP_CONTENT_TYPE_MIDI)
            plugin_buffer->buffers[p] = currentMidiOut = (float *) calloc(buffer_size, 1);
        else if (port->getPortDirection() == aap::AAP_PORT_DIRECTION_INPUT)
            plugin_buffer->buffers[p] = controlIn;
        else
            plugin_buffer->buffers[p] = dummyBuffer;
    }
    if (currentAudioOut)
        currentAudioIn = currentAudioOut;
    if (currentMidiOut)
        currentMidiIn = currentMidiOut;

    // prepare connections
    std::vector<int64_t> buffer_proxy;
    for(int i = 0; i < nPorts; i++)
        buffer_proxy.push_back((int64_t) plugin_buffer->buffers[i]);

    proxy->prepare(plugin_buffer->num_frames, nPorts, buffer_proxy);

    // prepare inputs
    memcpy(audioIn, wav, buffer_size);
    for (int i = 0; i < float_count; i++)
        controlIn[i] = 0.5;

    // activate, run, deactivate
    proxy->activate();

    proxy->process(0);
    for (int b = 0; b < wavLength; b += buffer_size) {
        // prepare inputs -audioIn
        memcpy(audioIn, ((char*) wav) + b, b + buffer_size < wavLength ? buffer_size : wavLength - b);
        proxy->process(0);
        memcpy(((char*) outWav) + b, currentAudioOut, b + buffer_size < wavLength ? buffer_size : wavLength - b);
    }

    proxy->deactivate();

    for (int p = 0; plugin_buffer->buffers[p]; p++)
        free(plugin_buffer->buffers[p]);
    free(plugin_buffer->buffers);
    delete plugin_buffer;

    proxy->destroy();

    free(audioIn);
    free(midiIn);
    free(dummyBuffer);

    return 0;
}

} // namespace aapremote


extern "C" {


int Java_org_androidaudioplugin_aaphostsample_AAPSampleInterop_runClientAAP(JNIEnv *env, jclass cls, jobject jBinder, jint sampleRate, jstring jPluginId, jbyteArray wav, jbyteArray outWav) {

    int wavLength = env->GetArrayLength(wav);
    void *wavBytes = calloc(wavLength, 1);
    env->GetByteArrayRegion(wav, 0, wavLength, (jbyte *) wavBytes);
    void *outWavBytes = calloc(wavLength, 1);

    jboolean dup;
    const char *pluginId_ = env->GetStringUTFChars(jPluginId, &dup);
    auto pluginId = strdup(pluginId_);
    env->ReleaseStringUTFChars(jPluginId, pluginId_);
    aap::PluginInformation *pluginInfo;
    int p = 0;
    while (local_plugin_infos[p]) {
        if (strcmp(local_plugin_infos[p]->getPluginID(), pluginId) == 0) {
            pluginInfo = local_plugin_infos[p];
            break;
        }
    }

    auto binder = AIBinder_fromJavaBinder(env, jBinder);
    auto proxy = new aidl::org::androidaudioplugin::BpAudioPluginInterface(ndk::SpAIBinder(binder));
    int ret = aapremote::runClientAAP(proxy, sampleRate, pluginInfo, wavBytes, wavLength, outWavBytes);
    env->SetByteArrayRegion(outWav, 0, wavLength, (jbyte*) outWavBytes);

    // FIXME: this causes crash?
    //delete proxy;

    free(wavBytes);
    free(outWavBytes);
    return ret;
}

} // extern "C"
