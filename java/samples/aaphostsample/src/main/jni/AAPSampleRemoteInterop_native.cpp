
#include <jni.h>
#include <android/binder_ibinder.h>
#include <android/binder_ibinder_jni.h>
#include <android/binder_interface_utils.h>
#include <android/binder_parcel.h>
#include <android/binder_parcel_utils.h>
#include <android/binder_status.h>
#include <android/binder_auto_utils.h>
#include <android/sharedmem.h>
#include <android/log.h>
#include <sys/mman.h>
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

int runClientAAP(aidl::org::androidaudioplugin::IAudioPluginInterface* proxy, int sampleRate, const aap::PluginInformation *pluginInfo, int wavLength, void *audioInBytesL, void *audioInBytesR, void *audioOutBytesL, void *audioOutBytesR)
{
    int buffer_size = 44100 * sizeof(float);
    int float_count = buffer_size / sizeof(float);

    /* instantiate plugins and connect ports */

    int audioInFD = ASharedMemory_create("audioInFD", buffer_size);
    int midiInFD = ASharedMemory_create("midiInFD", buffer_size);
    int controlInFD = ASharedMemory_create("controlInFD", buffer_size);
    int dummyBufferFD = ASharedMemory_create("dummyBufferFD", buffer_size);

    float *audioIn = (float *) mmap(nullptr, buffer_size, PROT_READ | PROT_WRITE, MAP_SHARED, audioInFD, 0);
    float *midiIn = (float *) mmap(nullptr, buffer_size, PROT_READ | PROT_WRITE, MAP_SHARED, midiInFD, 0);
    float *controlIn = (float *) mmap(nullptr, buffer_size, PROT_READ | PROT_WRITE, MAP_SHARED, controlInFD, 0);
    float *dummyBuffer = (float *) mmap(nullptr, buffer_size, PROT_READ | PROT_WRITE, MAP_SHARED, dummyBufferFD, 0);

    auto desc = pluginInfo;
    int nPorts = desc->getNumPorts();
    std::vector<int64_t> buffer_shm_fds;
    buffer_shm_fds.resize(nPorts, 0);

    float *currentAudioIn = audioIn, *currentAudioOut = NULL, *currentMidiIn = midiIn, *currentMidiOut = NULL;
    int64_t currentAudioInFD = audioInFD, currentAudioOutFD = 0, currentMidiInFD = midiInFD, currentMidiOutFD = 0;

    // enter processing...

    auto status = proxy->create(pluginInfo->getPluginID(), sampleRate);
    assert (status.isOk());

    auto plugin_buffer = new AndroidAudioPluginBuffer();
    plugin_buffer->num_frames = buffer_size / sizeof(float);
    plugin_buffer->buffers = (void **) calloc(nPorts + 1, sizeof(void *));
    for (int p = 0; p < nPorts; p++) {
        auto port = desc->getPort(p);
        if (port->getPortDirection() == aap::AAP_PORT_DIRECTION_INPUT &&
            port->getContentType() == aap::AAP_CONTENT_TYPE_AUDIO) {
            buffer_shm_fds[p] = currentAudioInFD;
            plugin_buffer->buffers[p] = currentAudioIn;
        }
        else if (port->getPortDirection() == aap::AAP_PORT_DIRECTION_OUTPUT &&
                 port->getContentType() == aap::AAP_CONTENT_TYPE_AUDIO) {
            buffer_shm_fds[p] = currentAudioOutFD = ASharedMemory_create(port->getName(), buffer_size);
            plugin_buffer->buffers[p] = currentAudioOut = (float *) mmap(nullptr, buffer_size, PROT_READ | PROT_WRITE, MAP_SHARED, buffer_shm_fds[p], 0);
        }
        else if (port->getPortDirection() == aap::AAP_PORT_DIRECTION_INPUT &&
                 port->getContentType() == aap::AAP_CONTENT_TYPE_MIDI) {
            buffer_shm_fds[p] = currentMidiInFD;
            plugin_buffer->buffers[p] = currentMidiIn;
        }
        else if (port->getPortDirection() == aap::AAP_PORT_DIRECTION_OUTPUT &&
                 port->getContentType() == aap::AAP_CONTENT_TYPE_MIDI) {
            buffer_shm_fds[p] = currentMidiOutFD = ASharedMemory_create(port->getName(), buffer_size);
            plugin_buffer->buffers[p] = currentMidiOut = (float *) mmap(nullptr, buffer_size, PROT_READ | PROT_WRITE, MAP_SHARED, buffer_shm_fds[p], 0);
        }
        else if (port->getPortDirection() == aap::AAP_PORT_DIRECTION_INPUT) {
            buffer_shm_fds[p] = controlInFD;
            plugin_buffer->buffers[p] = controlIn;
        }
        else {
            buffer_shm_fds[p] = dummyBufferFD;
            plugin_buffer->buffers[p] = dummyBuffer;
        }
    }
    if (currentAudioOut) {
        currentAudioIn = currentAudioOut;
        currentAudioInFD = currentAudioOutFD;
    }
    if (currentMidiOut) {
        currentMidiIn = currentMidiOut;
        currentMidiInFD = currentMidiOutFD;
    }

    // prepare connections
    proxy->prepare(plugin_buffer->num_frames, nPorts, buffer_shm_fds);

    // prepare inputs
    for (int i = 0; i < float_count; i++)
        controlIn[i] = 0.5;

    // activate, run, deactivate
    proxy->activate();

    for (int b = 0; b < wavLength; b += buffer_size) {
        int size = b + buffer_size < wavLength ? buffer_size : wavLength - b;
        // FIXME: handle ALL audio in bytes
        memcpy(audioIn, ((char*) audioInBytesL) + b, size);
        proxy->process(0);
        // FIXME: handle ALL audio out bytes
        memcpy(((char*) audioOutBytesL) + b, currentAudioOut, size);
    }

    proxy->deactivate();

    for (int p = 0; plugin_buffer->buffers[p]; p++)
        if(plugin_buffer->buffers[p] != nullptr
           && plugin_buffer->buffers[p] != dummyBuffer
           && plugin_buffer->buffers[p] != audioIn
           && plugin_buffer->buffers[p] != midiIn
           && plugin_buffer->buffers[p] != controlIn)
            munmap(plugin_buffer->buffers[p], buffer_size);
    free(plugin_buffer->buffers);
    delete plugin_buffer;

    proxy->destroy();

    munmap(audioIn, buffer_size);
    munmap(midiIn, buffer_size);
    munmap(controlIn, buffer_size);
    munmap(dummyBuffer, buffer_size);

    close(audioInFD);
    close(midiInFD);
    close(controlInFD);
    close(dummyBufferFD);

    return 0;
}

} // namespace aapremote


extern "C" {


int Java_org_androidaudioplugin_aaphostsample_AAPSampleInterop_runClientAAP(JNIEnv *env, jclass cls, jobject jBinder, jint sampleRate, jstring jPluginId, jbyteArray audioInL, jbyteArray audioInR, jbyteArray audioOutL, jbyteArray audioOutR)
{
    int wavLength = env->GetArrayLength(audioInL);
    assert(wavLength == env->GetArrayLength(audioInR));
    assert(wavLength == env->GetArrayLength(audioOutL));
    assert(wavLength == env->GetArrayLength(audioOutR));

    void *audioInBytesL = calloc(wavLength, 1);
    void *audioInBytesR = calloc(wavLength, 1);
    env->GetByteArrayRegion(audioInL, 0, wavLength, (jbyte *) audioInBytesL);
    env->GetByteArrayRegion(audioInR, 0, wavLength, (jbyte *) audioInBytesR);
    void *audioOutBytesL = calloc(wavLength, 1);
    void *audioOutBytesR = calloc(wavLength, 1);

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
    int ret = aapremote::runClientAAP(proxy, sampleRate, pluginInfo, wavLength, audioInBytesL, audioInBytesR, audioOutBytesL, audioOutBytesR);

    env->SetByteArrayRegion(audioOutL, 0, wavLength, (jbyte*) audioOutBytesL);
    env->SetByteArrayRegion(audioOutR, 0, wavLength, (jbyte*) audioOutBytesR);

    // FIXME: this causes crash?
    //delete proxy;

    free(audioInBytesL);
    free(audioInBytesR);
    free(audioOutBytesL);
    free(audioOutBytesR);
    return ret;
}

} // extern "C"
