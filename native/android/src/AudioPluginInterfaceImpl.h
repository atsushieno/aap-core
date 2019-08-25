//
// Created by atsushi on 19/08/26.
//

#ifndef ANDROIDAUDIOPLUGIN_AUDIOPLUGININTERFACEIMPL_H
#define ANDROIDAUDIOPLUGIN_AUDIOPLUGININTERFACEIMPL_H

#include <android/binder_ibinder.h>
#include <android/binder_ibinder_jni.h>
#include <android/binder_interface_utils.h>
#include <android/binder_parcel.h>
#include <android/binder_parcel_utils.h>
#include <android/binder_status.h>
#include <android/binder_auto_utils.h>
#include "aap/android-audio-plugin-host.hpp"

extern "C" {
extern aap::PluginInformation **local_plugin_infos;
}

namespace aap {

class AudioPluginInterfaceImpl : public aidl::org::androidaudioplugin::BnAudioPluginInterface {
    aap::PluginHost *host;
    aap::PluginInstance *instance;
    std::vector<int64_t> sharedMemoryFDs;
    AndroidAudioPluginBuffer buffer;
    int sample_rate;
    int current_buffer_size;

public:

    AudioPluginInterfaceImpl(int sampleRate)
            : sample_rate(sampleRate) {
        host = new PluginHost(local_plugin_infos);
        buffer.num_frames = 0;
        buffer.buffers = nullptr;
        instance = nullptr;
    }

    ::ndk::ScopedAStatus create(const std::string &in_pluginId, int32_t in_sampleRate) override {
        if (instance)
            return ndk::ScopedAStatus(
                    AStatus_fromServiceSpecificErrorWithMessage(-1, "Already instantiated"));
        instance = host->instantiatePlugin(in_pluginId.data());
        return ndk::ScopedAStatus::ok();
    }

    ::ndk::ScopedAStatus isPluginAlive(bool *_aidl_return) override {
        *_aidl_return = true;
        return ndk::ScopedAStatus::ok();
    }

    void freeBuffers() {
        if (buffer.buffers)
            for (int i = 0; i < sharedMemoryFDs.size(); i++)
                if (buffer.buffers[i])
                    munmap(buffer.buffers[i], current_buffer_size);
    }

    void resetBuffers(int frameCount, const std::vector<int64_t> &newFDs) {
        __android_log_print(ANDROID_LOG_INFO, "!!!AAPDEBUG!!!", "resetBuffers/");
        if (sharedMemoryFDs.size() != newFDs.size()) {
            freeBuffers();
            sharedMemoryFDs.resize(newFDs.size(), 0);
        }

        buffer.num_frames = frameCount;
        current_buffer_size = buffer.num_frames * sizeof(float);
        int n = newFDs.size();
        if (!buffer.buffers)
            buffer.buffers = (void **) calloc(sizeof(void *), n + 1);
        for (int i = 0; i < n; i++) {
            if (sharedMemoryFDs[i] != newFDs[i]) { // shm FD has changed
                if (buffer.buffers[i])
                    munmap(buffer.buffers[i], current_buffer_size);
                sharedMemoryFDs[i] = newFDs[i];
                buffer.buffers[i] = mmap(nullptr, current_buffer_size, PROT_READ | PROT_WRITE,
                                         MAP_SHARED, sharedMemoryFDs[i], 0);
                assert(buffer.buffers[i] != nullptr);
            }
        }
        buffer.buffers[newFDs.size()] = nullptr;

        for (int i = 0; i < n; i++)
            assert(buffer.buffers[i] != nullptr);
        __android_log_print(ANDROID_LOG_INFO, "!!!AAPDEBUG!!!", "/resetBuffers");
    }

    ::ndk::ScopedAStatus prepare(int32_t in_frameCount, int32_t in_portCount,
                                 const std::vector<int64_t> &in_sharedMemoryFDs) override {
        __android_log_print(ANDROID_LOG_INFO, "!!!AAPDEBUG!!!", "prepare");
        resetBuffers(in_frameCount, in_sharedMemoryFDs);
        instance->prepare(sample_rate, in_frameCount, &buffer);
        __android_log_print(ANDROID_LOG_INFO, "!!!AAPDEBUG!!!", "prepare done");
        return ndk::ScopedAStatus::ok();
    }

    ::ndk::ScopedAStatus activate() override {
        __android_log_print(ANDROID_LOG_INFO, "!!!AAPDEBUG!!!", "activate");
        instance->activate();
        return ndk::ScopedAStatus::ok();
    }

    ::ndk::ScopedAStatus process(int32_t in_timeoutInNanoseconds) override {
        __android_log_print(ANDROID_LOG_INFO, "!!!AAPDEBUG!!!", "process");
        instance->process(&buffer, in_timeoutInNanoseconds);
        return ndk::ScopedAStatus::ok();
    }

    ::ndk::ScopedAStatus deactivate() override {
        instance->deactivate();
        return ndk::ScopedAStatus::ok();
    }

    ::ndk::ScopedAStatus getStateSize(int32_t *_aidl_return) override {
        *_aidl_return = instance->getStateSize();
        return ndk::ScopedAStatus::ok();
    }

    ::ndk::ScopedAStatus getState(int64_t in_sharedMemoryFD) override {
        auto size = instance->getStateSize();
        auto dst = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, in_sharedMemoryFD, 0);
        memcpy(dst, instance->getState(), size);
        munmap(dst, size);
        return ndk::ScopedAStatus::ok();
    }

    ::ndk::ScopedAStatus setState(int64_t in_sharedMemoryFD, int32_t in_size) override {
        auto src = mmap(nullptr, in_size, PROT_READ | PROT_WRITE, MAP_SHARED, in_sharedMemoryFD, 0);
        instance->setState(src, 0, in_size);
        munmap(src, in_size);
        return ndk::ScopedAStatus::ok();
    }

    ::ndk::ScopedAStatus destroy() override {
        instance->dispose();
        freeBuffers();
        return ndk::ScopedAStatus::ok();
    }
};


#endif //ANDROIDAUDIOPLUGIN_AUDIOPLUGININTERFACEIMPL_H

} // namespace aap