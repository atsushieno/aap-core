#if false

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

// FIXME: sort out final library header structures.
namespace aap {
    extern aap::PluginInformation *pluginInformation_fromJava(JNIEnv *env, jobject pluginInformation);
    extern const char *strdup_fromJava(JNIEnv *env, jstring s);
}


namespace aapremote {

int runClientAAP(aidl::org::androidaudioplugin::IAudioPluginInterface* proxy, int sampleRate, const char* pluginId, int wavLength, void *audioInBytesL, void *audioInBytesR, void *audioOutBytesL, void *audioOutBytesR, float* parameters)
{
    assert(proxy != nullptr);

    aap::PluginInformation *pluginInfo{nullptr};
    auto plugins = aap::getPluginHostPAL()->getPluginListCache();
    for (int p = 0; plugins[p] != nullptr; p++) {
        if (strcmp(plugins[p]->getPluginID().data(), pluginId) == 0) {
            pluginInfo = plugins[p];
            break;
        }
    }
    assert(pluginInfo != nullptr);

    size_t buffer_size = 44100 * sizeof(float);
    int float_count = buffer_size / sizeof(float);

    /* instantiate plugins and connect ports */
    auto desc = pluginInfo;
    int nPorts = desc->getNumPorts();

    // enter processing...

    int32_t instanceID;
    auto status = proxy->beginCreate(pluginInfo->getPluginID(), sampleRate, &instanceID);
    assert (status.isOk());
    status.set(proxy->endCreate(instanceID).get());
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
        memset(ptr, 0, buffer_size);
        assert(ptr != nullptr);
        plugin_buffer->buffers[i] = ptr;
    }

    int audioInChannelMapped = 0, audioOutChannelMapped = 0;
    int audioInPortL = -1, audioInPortR = -1, audioOutPortL = -1, audioOutPortR = -1;
    int midiInPort = -1, midiOutPort = -1;
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
            midiInPort = p;
        }
        else if (port->getPortDirection() == aap::AAP_PORT_DIRECTION_OUTPUT &&
                 port->getContentType() == aap::AAP_CONTENT_TYPE_MIDI) {
            midiOutPort = p;
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
        auto status2 = proxy->prepareMemory(instanceID, i, fd);
        assert (status2.isOk());
		fds.push_back(std::move(fd));
    }
    auto status2x = proxy->prepare(instanceID, plugin_buffer->num_frames, nPorts);
    assert (status2x.isOk());

    // activate, run, deactivate
    auto status3 = proxy->activate(instanceID);
    assert (status3.isOk());

	// prepare control inputs - dummy
	for (int p = 0; p < buffer_shm_fds.size(); p++) {
	    if (p == midiInPort) {
            auto mb = (uint8_t*) plugin_buffer->buffers[p];
            int length = 20;
            int16_t fps = -30;
            int16_t ticksPerFrame = 100;
            *((int*) mb) = 0xF000 | ticksPerFrame | (fps << 8); // 30fps, 100 ticks per frame
            //*(int*) mb = 480;
            *((int*) mb + 1) = length;
            mb[8] = 0; // program change
            mb[9] = 0xC0;
            mb[10] = 0;
            mb[11] = 0; // note on
            mb[12] = 0x90;
            mb[13] = 0x39;
            mb[14] = 0x70;
            mb[15] = 0; // note on
            mb[16] = 0x90;
            mb[17] = 0x3D;
            mb[18] = 0x70;
            mb[19] = 0; // note on
            mb[20] = 0x90;
            mb[21] = 0x40;
            mb[22] = 0x70;
            mb[23] = 0x80; // note off
            mb[24] = 0x70;
            mb[25] = 0x80;
            mb[26] = 0x45;
            mb[27] = 0x00;
        }
		else if (pluginInfo->getPort(p)->getContentType() == aap::AAP_CONTENT_TYPE_UNDEFINED)
			for (int i = 0; i < float_count; i++)
				((float*) plugin_buffer->buffers[p])[i] = parameters[i];
	}

	for (int b = 0; b < wavLength; b += buffer_size) {
        int size = b + buffer_size < wavLength ? buffer_size : wavLength - b;
        // FIXME: handle more channels
        if (audioInPortL >= 0)
            memcpy(plugin_buffer->buffers[audioInPortL], ((char*) audioInBytesL) + b, size);
        if (audioInPortR >= 0)
            memcpy(plugin_buffer->buffers[audioInPortR], ((char*) audioInBytesR) + b, size);
        auto status4 = proxy->process(instanceID, 0);
        assert (status4.isOk());
        // FIXME: handle more channels
        if (audioOutBytesL != nullptr && (audioOutPortL >= 0))
            memcpy(((char*) audioOutBytesL) + b, plugin_buffer->buffers[audioOutPortL], size);
        if (audioOutBytesR != nullptr && (audioOutPortR >= 0))
            memcpy(((char*) audioOutBytesR) + b, plugin_buffer->buffers[audioOutPortR], size);
        else
            memcpy(((char*) audioOutBytesR) + b, plugin_buffer->buffers[audioOutPortL], size);
    }

    auto status5 = proxy->deactivate(instanceID);
    assert (status5.isOk());

    for (int p = 0; p < plugin_buffer->num_buffers; p++)
        if(plugin_buffer->buffers[p] != nullptr)
            munmap(plugin_buffer->buffers[p], buffer_size);
    free(plugin_buffer->buffers);
    delete plugin_buffer;

    auto status6 = proxy->destroy(instanceID);
    assert (status6.isOk());

    for (int p = 0; p < nPorts; p++)
        close(buffer_shm_fds[p]);

    return 0;
}

} // namespace aapremote


extern "C" {


int Java_org_androidaudioplugin_aaphostsample_AAPSampleInterop_runClientAAP(JNIEnv *env, jclass cls, jobject jBinder, jint sampleRate, jstring jPluginId, jbyteArray audioInL, jbyteArray audioInR, jbyteArray audioOutL, jbyteArray audioOutR, jfloatArray jParameters)
{
	// NOTE: setting a breakpoint in this method might cause SIGSEGV.
	// That's of course non-user code issue but you would't like to waste your time on diagnosing it...

    assert(audioInL != nullptr);
    assert(audioInR != nullptr);
    assert(audioOutL != nullptr);
    assert(audioOutR != nullptr);
    assert(jParameters != nullptr);

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
    int numParameters = env->GetArrayLength(jParameters);
    float *parameters = (float*) calloc(numParameters, sizeof(float));
    assert(parameters != nullptr);
    env->GetFloatArrayRegion(jParameters, 0, numParameters, parameters);

    assert(jPluginId != nullptr);
    jboolean dup;
    const char *pluginId_ = env->GetStringUTFChars(jPluginId, &dup);
    auto pluginId = strdup(pluginId_);
    env->ReleaseStringUTFChars(jPluginId, pluginId_);

    auto binder = AIBinder_fromJavaBinder(env, jBinder);
    ndk::SpAIBinder spBinder{binder};
    auto proxy = aidl::org::androidaudioplugin::IAudioPluginInterface::fromBinder(spBinder);
    int ret = aapremote::runClientAAP(proxy.get(), sampleRate, pluginId, wavLength, audioInBytesL, audioInBytesR, audioOutBytesL, audioOutBytesR, parameters);

    env->SetByteArrayRegion(audioOutL, 0, wavLength, (jbyte*) audioOutBytesL);
    env->SetByteArrayRegion(audioOutR, 0, wavLength, (jbyte*) audioOutBytesR);

    // FIXME: this causes crash?
    //delete proxy;

    free(audioInBytesL);
    free(audioInBytesR);
    free(audioOutBytesL);
    free(audioOutBytesR);
    free(parameters);
    return ret;
}

} // extern "C"

#endif
