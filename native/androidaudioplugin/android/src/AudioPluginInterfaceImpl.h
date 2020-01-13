//
// Created by atsushi on 19/08/26.
//

#ifndef ANDROIDAUDIOPLUGIN_AUDIOPLUGININTERFACEIMPL_H
#define ANDROIDAUDIOPLUGIN_AUDIOPLUGININTERFACEIMPL_H

#include <android/sharedmem.h>
#include <android/binder_ibinder.h>
#include <android/binder_ibinder_jni.h>
#include <android/binder_interface_utils.h>
#include <android/binder_parcel.h>
#include <android/binder_parcel_utils.h>
#include <android/binder_status.h>
#include <android/binder_auto_utils.h>
#include "aap/android-audio-plugin-host.hpp"

namespace aap {

class AudioPluginInterfaceImpl : public aidl::org::androidaudioplugin::BnAudioPluginInterface {
    aap::PluginHost *host;
    aap::PluginInstance *instance;
    std::vector<int64_t> sharedMemoryFDs{};
    AndroidAudioPluginBuffer buffer;
    int sample_rate;
    int current_buffer_size;

public:

    AudioPluginInterfaceImpl(int sampleRate)
            : sample_rate(sampleRate) {
        host = new PluginHost();
        buffer.num_frames = 0;
        buffer.buffers = nullptr;
        instance = nullptr;
        sharedMemoryFDs.clear();
    }
    virtual ~AudioPluginInterfaceImpl() {
        delete host;
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

    bool resetBuffers(int frameCount, const std::vector<int64_t> &newFDs) {
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
            if (buffer.buffers[i])
                munmap(buffer.buffers[i], current_buffer_size);
            buffer.buffers[i] = mmap(nullptr, current_buffer_size, PROT_READ | PROT_WRITE,
                                     MAP_SHARED, sharedMemoryFDs[i], 0);
            if (buffer.buffers[i] == MAP_FAILED) {
                int err = errno;
                __android_log_print(ANDROID_LOG_ERROR, "AndroidAudioPlugin",
                                    "mmap failed at buffer %d. errno = %d", i, err);
                return false;
            }
            assert(buffer.buffers[i] != nullptr);
        }
        buffer.buffers[newFDs.size()] = nullptr;
        return true;
    }

    // Since AIDL does not support sending List of ParcelFileDescriptor it is sent one by one.
    // Here we just cache the FDs, and process them later at prepare().
    ::ndk::ScopedAStatus prepareMemory(int32_t index, const ::ndk::ScopedFileDescriptor& in_sharedMemoryFD) override {
        auto fd = dup(in_sharedMemoryFD.get());
    	while (sharedMemoryFDs.size() <= index)
    		sharedMemoryFDs.push_back(0); // dummy
    	sharedMemoryFDs[index] = fd;
        return ndk::ScopedAStatus::ok();
    }

    ::ndk::ScopedAStatus prepare(int32_t in_frameCount, int32_t in_portCount) override {
        if (!resetBuffers(in_frameCount, sharedMemoryFDs)) {
            __android_log_print(ANDROID_LOG_ERROR, "AndroidAudioPlugin", "Failed to prepare shared memory buffers.");
            return ndk::ScopedAStatus{AStatus_fromServiceSpecificError(2)}; // FIXME : define error codes
        }
        instance->prepare(sample_rate, in_frameCount, &buffer);
        return ndk::ScopedAStatus::ok();
    }

    ::ndk::ScopedAStatus activate() override {
        instance->activate();
        return ndk::ScopedAStatus::ok();
    }

    ::ndk::ScopedAStatus process(int32_t in_timeoutInNanoseconds) override {
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

    ::ndk::ScopedAStatus getState(const ::ndk::ScopedFileDescriptor& in_sharedMemoryFD) override {
        auto size = instance->getStateSize();
        auto dst = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, dup(in_sharedMemoryFD.get()), 0);
        memcpy(dst, instance->getState(), size);
        munmap(dst, size);
        return ndk::ScopedAStatus::ok();
    }

    ::ndk::ScopedAStatus setState(const ::ndk::ScopedFileDescriptor& in_sharedMemoryFD, int32_t in_size) override {
        auto src = mmap(nullptr, in_size, PROT_READ | PROT_WRITE, MAP_SHARED, dup(in_sharedMemoryFD.get()), 0);
        instance->setState(src, 0, in_size);
        munmap(src, in_size);
        return ndk::ScopedAStatus::ok();
    }

    ::ndk::ScopedAStatus destroy() override {
        instance->dispose();
        delete instance;
        instance = nullptr;
        freeBuffers();
        return ndk::ScopedAStatus::ok();
    }
};


#endif //ANDROIDAUDIOPLUGIN_AUDIOPLUGININTERFACEIMPL_H

} // namespace aap