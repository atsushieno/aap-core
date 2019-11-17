
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

aap::PluginInformation **all_plugin_infos{nullptr};

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

    size_t buffer_size = 44100 * sizeof(float);
    int float_count = buffer_size / sizeof(float);

    /* instantiate plugins and connect ports */
    auto desc = pluginInfo;
    int nPorts = desc->getNumPorts();

    // enter processing...

    auto status = proxy->create(pluginInfo->getPluginID(), sampleRate);
    assert (status.isOk());

    auto plugin_buffer = new AndroidAudioPluginBuffer();
    plugin_buffer->num_frames = buffer_size / sizeof(float);
    plugin_buffer->buffers = (void **) calloc(nPorts + 1, sizeof(void *));
    std::vector<int> buffer_shm_fds;

    // Unlike local audio connectors, those shared memory buffers cannot be simply bypassed
    // to any port. We simply create shm for every port, and mmap them.
    for (int i = 0; i < nPorts; i++) {
        int fd = ASharedMemory_create(nullptr, buffer_size);
        assert(fd >= 0);
        buffer_shm_fds.push_back(fd);
        void* ptr = mmap(nullptr, buffer_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        assert(ptr != nullptr);
        plugin_buffer->buffers[i] = ptr;
    }

    int audioInChannelMapped = 0, audioOutChannelMapped = 0;
    int audioInPortL = -1, audioInPortR = -1, audioOutPortL = -1, audioOutPortR = -1;
    for (int p = 0; p < nPorts; p++) {
        auto port = desc->getPort(p);
        if (port->getPortDirection() == aap::AAP_PORT_DIRECTION_INPUT &&
            port->getContentType() == aap::AAP_CONTENT_TYPE_AUDIO) {
            if (audioInChannelMapped)
                audioInPortR = p;
            else
                audioInPortL = p;
            audioInChannelMapped++;
        }
        else if (port->getPortDirection() == aap::AAP_PORT_DIRECTION_OUTPUT &&
                 port->getContentType() == aap::AAP_CONTENT_TYPE_AUDIO) {
            if (audioOutChannelMapped)
                audioOutPortR = p;
            else
                audioOutPortL = p;
            audioOutChannelMapped++;
        }
        else if (port->getPortDirection() == aap::AAP_PORT_DIRECTION_INPUT &&
                 port->getContentType() == aap::AAP_CONTENT_TYPE_MIDI) {
            // FIXME: support it
        }
        else if (port->getPortDirection() == aap::AAP_PORT_DIRECTION_OUTPUT &&
                 port->getContentType() == aap::AAP_CONTENT_TYPE_MIDI) {
            // FIXME: support it
        }
        else if (port->getPortDirection() == aap::AAP_PORT_DIRECTION_INPUT) {
            // FIXME: support it
        }
        else {
            // FIXME: support it
        }
    }

    // prepare connections
    std::vector<::ndk::ScopedFileDescriptor> fds;
    for (int i = 0; i < buffer_shm_fds.size(); i++) {
        ::ndk::ScopedFileDescriptor fd;
        fd.set(buffer_shm_fds[i]);
        auto status2 = proxy->prepareMemory(i, fd);
        fds.push_back(std::move(fd));
        assert (status2.isOk());
    }
    auto status2x = proxy->prepare(plugin_buffer->num_frames, nPorts);
    assert (status2x.isOk());

    // activate, run, deactivate
    auto status3 = proxy->activate();
    assert (status3.isOk());

    for (int b = 0; b < wavLength; b += buffer_size) {
        int size = b + buffer_size < wavLength ? buffer_size : wavLength - b;
        // FIXME: handle more channels
        if (audioInPortL >= 0)
            memcpy(plugin_buffer->buffers[audioInPortL], ((char*) audioInBytesL) + b, size);
        if (audioInPortR >= 0)
            memcpy(plugin_buffer->buffers[audioInPortR], ((char*) audioInBytesR) + b, size);
        auto status4 = proxy->process(0);
        assert (status4.isOk());
        // FIXME: handle more channels
        if (audioOutBytesL != nullptr && audioOutPortL >= 0)
            memcpy(((char*) audioOutBytesL) + b, plugin_buffer->buffers[audioOutPortL], size);
        if (audioOutBytesR != nullptr && audioOutPortL >= 0)
            memcpy(((char*) audioOutBytesR) + b, plugin_buffer->buffers[audioOutPortR], size);
    }

    auto status5 = proxy->deactivate();
    assert (status5.isOk());

    for (int p = 0; plugin_buffer->buffers[p]; p++)
        if(plugin_buffer->buffers[p] != nullptr)
            munmap(plugin_buffer->buffers[p], buffer_size);
    free(plugin_buffer->buffers);
    delete plugin_buffer;

    auto status6 = proxy->destroy();
    assert (status6.isOk());

    for (int p = 0; p < nPorts; p++)
        close(buffer_shm_fds[p]);

    return 0;
}

} // namespace aapremote


extern "C" {


void Java_org_androidaudioplugin_aaphostsample_AAPSampleInterop_initialize(JNIEnv *env, jclass cls, jobjectArray jPluginInfos)
{
	assert(all_plugin_infos == nullptr);
	jsize infoSize = env->GetArrayLength(jPluginInfos);
	all_plugin_infos = (aap::PluginInformation **) calloc(sizeof(aap::PluginInformation *), infoSize + 1);
	for (int i = 0; i < infoSize; i++) {
		auto jPluginInfo = (jobject) env->GetObjectArrayElement(jPluginInfos, i);
		all_plugin_infos[i] = aap::pluginInformation_fromJava(env, jPluginInfo);
	}
	all_plugin_infos[infoSize] = nullptr;
}

int Java_org_androidaudioplugin_aaphostsample_AAPSampleInterop_runClientAAP(JNIEnv *env, jclass cls, jobject jBinder, jint sampleRate, jstring jPluginId, jbyteArray audioInL, jbyteArray audioInR, jbyteArray audioOutL, jbyteArray audioOutR)
{
	// NOTE: setting a breakpoint in this method might cause SIGSEGV.
	// That's of course non-user code issue but you would't like to waste your time on diagnosing it...

    assert(all_plugin_infos != nullptr);
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
    aap::PluginInformation *pluginInfo{nullptr};
    for (int p = 0; all_plugin_infos[p] != nullptr; p++) {
        if (strcmp(all_plugin_infos[p]->getPluginID().data(), pluginId) == 0) {
            pluginInfo = all_plugin_infos[p];
            break;
        }
    }
    assert(pluginInfo != nullptr);

    auto binder = AIBinder_fromJavaBinder(env, jBinder);
    ndk::SpAIBinder spBinder{binder};
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