
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
#include "aidl/org/androidaudioplugin/AudioPluginInterface.h"
#include "aidl/org/androidaudioplugin/BpAudioPluginInterface.h"
#include "aap/android-audio-plugin-host.hpp"

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
    assert(proxy != nullptr);
    assert(pluginInfo != nullptr);

    int buffer_size = 44100 * sizeof(float);
    int float_count = buffer_size / sizeof(float);

    /* instantiate plugins and connect ports */

    int audioInLFD = ASharedMemory_create("audioInLFD", buffer_size);
    int audioInRFD = ASharedMemory_create("audioInRFD", buffer_size);
    int midiInFD = ASharedMemory_create("midiInFD", buffer_size);
    int controlInFD = ASharedMemory_create("controlInFD", buffer_size);
    int dummyBufferFD = ASharedMemory_create("dummyBufferFD", buffer_size);

    // FIXME: process more channels
    float *audioInL = (float *) mmap(nullptr, buffer_size, PROT_READ | PROT_WRITE, MAP_SHARED, audioInLFD, 0);
    assert(audioInL != nullptr);
    float *audioInR = (float *) mmap(nullptr, buffer_size, PROT_READ | PROT_WRITE, MAP_SHARED, audioInRFD, 0);
    assert(audioInR != nullptr);
    float *midiIn = (float *) mmap(nullptr, buffer_size, PROT_READ | PROT_WRITE, MAP_SHARED, midiInFD, 0);
    assert(midiIn != nullptr);
    float *controlIn = (float *) mmap(nullptr, buffer_size, PROT_READ | PROT_WRITE, MAP_SHARED, controlInFD, 0);
    assert(controlIn != nullptr);
    float *dummyBuffer = (float *) mmap(nullptr, buffer_size, PROT_READ | PROT_WRITE, MAP_SHARED, dummyBufferFD, 0);
    assert(dummyBuffer != nullptr);

    auto desc = pluginInfo;
    int nPorts = desc->getNumPorts();
    std::vector<int64_t> buffer_shm_fds;
    buffer_shm_fds.resize(nPorts, 0);

    float *currentAudioInL = audioInL, *currentAudioInR = audioInR, *currentAudioOutL = NULL, *currentAudioOutR = NULL,
        *currentMidiIn = midiIn, *currentMidiOut = NULL;
    int64_t currentAudioInLFD = audioInLFD, currentAudioInRFD = audioInRFD, currentAudioOutLFD = 0, currentAudioOutRFD = 0,
        currentMidiInFD = midiInFD, currentMidiOutFD = 0;

    // enter processing...

    auto status = proxy->create(pluginInfo->getPluginID(), sampleRate);
    assert (status.isOk());

    auto plugin_buffer = new AndroidAudioPluginBuffer();
    plugin_buffer->num_frames = buffer_size / sizeof(float);
    plugin_buffer->buffers = (void **) calloc(nPorts + 1, sizeof(void *));
    int audioInChannelMapped = 0, audioOutChannelMapped = 0;
    for (int p = 0; p < nPorts; p++) {
        auto port = desc->getPort(p);
        if (port->getPortDirection() == aap::AAP_PORT_DIRECTION_INPUT &&
            port->getContentType() == aap::AAP_CONTENT_TYPE_AUDIO) {
            buffer_shm_fds[p] = audioInChannelMapped > 0 ? currentAudioInRFD : currentAudioInLFD;
            plugin_buffer->buffers[p] = audioInChannelMapped > 0 ? currentAudioInR : currentAudioInL;
            audioInChannelMapped++;
        }
        else if (port->getPortDirection() == aap::AAP_PORT_DIRECTION_OUTPUT &&
                 port->getContentType() == aap::AAP_CONTENT_TYPE_AUDIO) {
            auto s = ASharedMemory_create(port->getName(), buffer_size);
            if (audioOutChannelMapped > 0)
                currentAudioOutRFD = buffer_shm_fds[p] = s;
            else
                currentAudioOutLFD = buffer_shm_fds[p] = s;
            auto b = (float *) mmap(nullptr, buffer_size, PROT_READ | PROT_WRITE, MAP_SHARED, buffer_shm_fds[p], 0);
            assert(b != nullptr);
            if (audioOutChannelMapped > 0)
                plugin_buffer->buffers[p] = currentAudioOutR = b;
            else
                plugin_buffer->buffers[p] = currentAudioOutL = b;
            audioOutChannelMapped++;
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
            assert(currentMidiOut != nullptr);
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
    if (currentAudioOutL) {
        currentAudioInL = currentAudioOutL;
        currentAudioInLFD = currentAudioOutLFD;
    }
    if (currentAudioOutR) {
        currentAudioInR = currentAudioOutR;
        currentAudioInRFD = currentAudioOutRFD;
    }
    if (currentMidiOut) {
        currentMidiIn = currentMidiOut;
        currentMidiInFD = currentMidiOutFD;
    }

    // prepare connections
    auto status2 = proxy->prepare(plugin_buffer->num_frames, nPorts, buffer_shm_fds);
    assert (status2.isOk());

    // prepare inputs
    for (int i = 0; i < float_count; i++)
        controlIn[i] = 0.5;

    // activate, run, deactivate
    auto status3 = proxy->activate();
    assert (status3.isOk());

    for (int b = 0; b < wavLength; b += buffer_size) {
        int size = b + buffer_size < wavLength ? buffer_size : wavLength - b;
        //__android_log_print(ANDROID_LOG_INFO, "!!!AAPDEBUG!!!", "b %d size %d bs %d wl %d", b, size, buffer_size, wavLength);
        // FIXME: handle more channels
        if (audioInBytesL)
            memcpy(audioInL, ((char*) audioInBytesL) + b, size);
        if (audioInBytesR)
            memcpy(audioInR, ((char*) audioInBytesR) + b, size);
        auto status4 = proxy->process(0);
        assert (status4.isOk());
        // FIXME: handle more channels
        if (audioOutBytesL)
            memcpy(((char*) audioOutBytesL) + b, currentAudioOutL, size);
        if (audioOutBytesR)
            memcpy(((char*) audioOutBytesR) + b, currentAudioOutR, size);
    }

    auto status5 = proxy->deactivate();
    assert (status5.isOk());

    for (int p = 0; plugin_buffer->buffers[p]; p++)
        if(plugin_buffer->buffers[p] != nullptr
           && plugin_buffer->buffers[p] != dummyBuffer
           && plugin_buffer->buffers[p] != audioInL
           && plugin_buffer->buffers[p] != audioInR
           && plugin_buffer->buffers[p] != midiIn
           && plugin_buffer->buffers[p] != controlIn)
            munmap(plugin_buffer->buffers[p], buffer_size);
    free(plugin_buffer->buffers);
    delete plugin_buffer;

    auto status6 = proxy->destroy();
    assert (status6.isOk());

    munmap(audioInL, buffer_size);
    munmap(audioInR, buffer_size);
    munmap(midiIn, buffer_size);
    munmap(controlIn, buffer_size);
    munmap(dummyBuffer, buffer_size);

    close(audioInLFD);
    close(audioInRFD);
    close(midiInFD);
    close(controlInFD);
    close(dummyBufferFD);

    return 0;
}

} // namespace aapremote


extern "C" {


int Java_org_androidaudioplugin_aaphostsample_AAPSampleInterop_runClientAAP(JNIEnv *env, jclass cls, jobject jBinder, jint sampleRate, jstring jPluginId, jbyteArray audioInL, jbyteArray audioInR, jbyteArray audioOutL, jbyteArray audioOutR)
{
    assert(local_plugin_infos != nullptr);
    assert(audioInL != nullptr);
    assert(audioInR != nullptr);
    assert(audioOutL != nullptr);
    assert(audioOutR != nullptr);

    int wavLength = env->GetArrayLength(audioInL);
    assert(wavLength == env->GetArrayLength(audioInR));
    assert(wavLength == env->GetArrayLength(audioOutL));
    assert(wavLength == env->GetArrayLength(audioOutR));

    void *audioInBytesL = calloc(wavLength, 1);
    assert(audioInBytesL != nullptr);
    void *audioInBytesR = calloc(wavLength, 1);
    assert(audioInBytesR != nullptr);
    env->GetByteArrayRegion(audioInL, 0, wavLength, (jbyte *) audioInBytesL);
    env->GetByteArrayRegion(audioInR, 0, wavLength, (jbyte *) audioInBytesR);
    void *audioOutBytesL = calloc(wavLength, 1);
    assert(audioOutBytesL != nullptr);
    void *audioOutBytesR = calloc(wavLength, 1);
    assert(audioOutBytesR != nullptr);

    assert(jPluginId != nullptr);
    jboolean dup;
    const char *pluginId_ = env->GetStringUTFChars(jPluginId, &dup);
    auto pluginId = strdup(pluginId_);
    env->ReleaseStringUTFChars(jPluginId, pluginId_);
    aap::PluginInformation *pluginInfo;
    for (int p = 0; local_plugin_infos[p] != nullptr; p++) {
        if (strcmp(local_plugin_infos[p]->getPluginID().data(), pluginId) == 0) {
            pluginInfo = local_plugin_infos[p];
            break;
        }
    }
    assert(pluginInfo != nullptr);

    auto binder = AIBinder_fromJavaBinder(env, jBinder);
    auto spBinder = ndk::SpAIBinder{binder};
    auto proxy = aidl::org::androidaudioplugin::IAudioPluginInterface::fromBinder(spBinder);
    int ret = aapremote::runClientAAP(proxy.get(), sampleRate, pluginInfo, wavLength, audioInBytesL, audioInBytesR, audioOutBytesL, audioOutBytesR);

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
