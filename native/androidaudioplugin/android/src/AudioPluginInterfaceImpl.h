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

class AudioPluginInstanceContext
{
	friend class AudioPluginInterfaceImpl;

    aap::PluginInstance *instance;
    std::vector<int64_t> sharedMemoryFDs{};
    AndroidAudioPluginBuffer buffer;
    int sample_rate;
    int current_buffer_size;
public:
    AudioPluginInstanceContext(int sampleRate, PluginInstance* pluginInstance)
    {
    	sample_rate = sampleRate;
        buffer.num_frames = 0;
        buffer.buffers = nullptr;
        instance = pluginInstance;
        sharedMemoryFDs.clear();
    }

    // Since AIDL does not support sending List of ParcelFileDescriptor it is sent one by one.
    // Here we just cache the FDs, and process them later at prepare().
    void prepareMemory(int32_t shmFDIndex, const ::ndk::ScopedFileDescriptor& sharedMemoryFD)
    {
        auto fd = dup(sharedMemoryFD.get());
        while (sharedMemoryFDs.size() <= shmFDIndex)
            sharedMemoryFDs.push_back(0); // dummy
        sharedMemoryFDs[shmFDIndex] = fd;
    }

    void freeBuffers()
    {
        if (buffer.buffers)
            for (int i = 0; i < sharedMemoryFDs.size(); i++)
                if (buffer.buffers[i])
                    munmap(buffer.buffers[i], current_buffer_size);
    }

    int resetBuffers(int frameCount, const std::vector<int64_t> &newFDs) {
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
                int err = errno; // FIXME : define error codes
                __android_log_print(ANDROID_LOG_ERROR, "AndroidAudioPlugin",
                                    "mmap failed at buffer %d. errno = %d", i, err);
                return err;
            }
            assert(buffer.buffers[i] != nullptr);
        }
        buffer.buffers[newFDs.size()] = nullptr;
        return 0;
    }

    int prepare(int32_t frameCount, int32_t portCount)
    {
        int ret = resetBuffers(frameCount, sharedMemoryFDs);
        if (ret != 0) {
            __android_log_print(ANDROID_LOG_ERROR, "AndroidAudioPlugin", "Failed to prepare shared memory buffers.");
            return ret;
        }
        instance->prepare(sample_rate, frameCount, &buffer);
        return 0;
    }

    void getState(const ::ndk::ScopedFileDescriptor& in_sharedMemoryFD)
    {
        auto size = instance->getStateSize();
        auto dst = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, dup(in_sharedMemoryFD.get()), 0);
        memcpy(dst, instance->getState(), size);
        munmap(dst, size);
    }

    void setState(const ::ndk::ScopedFileDescriptor& in_sharedMemoryFD, int32_t in_size)
    {
        auto src = mmap(nullptr, in_size, PROT_READ | PROT_WRITE, MAP_SHARED, dup(in_sharedMemoryFD.get()), 0);
        instance->setState(src, 0, in_size);
        munmap(src, in_size);
    }

    void destroy()
    {
        instance->dispose();
        delete instance;
        instance = nullptr;
        freeBuffers();
    }
};

class AudioPluginInterfaceImpl : public aidl::org::androidaudioplugin::BnAudioPluginInterface {
    aap::PluginHost *host;
    std::vector<AudioPluginInstanceContext*> instances{};

public:

    AudioPluginInterfaceImpl()
    {
        host = new PluginHost();
    }

    virtual ~AudioPluginInterfaceImpl() {
        delete host;
    }

    ::ndk::ScopedAStatus create(const std::string& in_pluginId, int32_t in_sampleRate, int32_t* _aidl_return) override
    {
        auto ctx = new AudioPluginInstanceContext(in_sampleRate, host->instantiatePlugin(in_pluginId.data()));
        *_aidl_return = instances.size();
        for (int i = 0; i < instances.size(); i++) {
            if (instances[i] == nullptr) {
                *_aidl_return = i;
                break;
            }
        }
        if (*_aidl_return < instances.size())
            instances[*_aidl_return] = ctx;
        else
            instances.push_back(ctx);
        return ndk::ScopedAStatus::ok();
    }

    ::ndk::ScopedAStatus isPluginAlive(int32_t in_instanceID, bool* _aidl_return) override
    {
        assert(in_instanceID < instances.size());
        *_aidl_return = instances[in_instanceID] != nullptr;
        return ndk::ScopedAStatus::ok();
    }

    // Since AIDL does not support sending List of ParcelFileDescriptor it is sent one by one.
    // Here we just cache the FDs, and process them later at prepare().
    ::ndk::ScopedAStatus prepareMemory(int32_t in_instanceID, int32_t in_shmFDIndex, const ::ndk::ScopedFileDescriptor& in_sharedMemoryFD) override
    {
        assert(in_instanceID < instances.size());
        instances[in_instanceID]->prepareMemory(in_shmFDIndex, in_sharedMemoryFD);
        return ndk::ScopedAStatus::ok();
    }

    ::ndk::ScopedAStatus prepare(int32_t in_instanceID, int32_t in_frameCount, int32_t in_portCount) override
    {
        assert(in_instanceID < instances.size());
        int ret = instances[in_instanceID]->prepare(in_frameCount, in_portCount);

        return ret != 0 ? ndk::ScopedAStatus{AStatus_fromServiceSpecificError(ret)}
                        : ndk::ScopedAStatus::ok();
    }

    ::ndk::ScopedAStatus activate(int32_t in_instanceID) override
    {
        assert(in_instanceID < instances.size());
        instances[in_instanceID]->instance->activate();
        return ndk::ScopedAStatus::ok();
    }

    ::ndk::ScopedAStatus process(int32_t in_instanceID, int32_t in_timeoutInNanoseconds) override
    {
        assert(in_instanceID < instances.size());
        instances[in_instanceID]->instance->process(&instances[in_instanceID]->buffer, in_timeoutInNanoseconds);
        return ndk::ScopedAStatus::ok();
    }

    ::ndk::ScopedAStatus deactivate(int32_t in_instanceID) override
    {
        assert(in_instanceID < instances.size());
        instances[in_instanceID]->instance->deactivate();
        return ndk::ScopedAStatus::ok();
    }

    ::ndk::ScopedAStatus getStateSize(int32_t in_instanceID, int32_t *_aidl_return) override
    {
        assert(in_instanceID < instances.size());
        *_aidl_return = instances[in_instanceID]->instance->getStateSize();
        return ndk::ScopedAStatus::ok();
    }

    ::ndk::ScopedAStatus getState(int32_t in_instanceID, const ::ndk::ScopedFileDescriptor& in_sharedMemoryFD) override
    {
        assert(in_instanceID < instances.size());
        instances[in_instanceID]->getState(in_sharedMemoryFD);
        return ndk::ScopedAStatus::ok();
    }

    ::ndk::ScopedAStatus setState(int32_t in_instanceID, const ::ndk::ScopedFileDescriptor& in_sharedMemoryFD, int32_t in_size) override
    {
        assert(in_instanceID < instances.size());
        instances[in_instanceID]->setState(in_sharedMemoryFD, in_size);
        return ndk::ScopedAStatus::ok();
    }

    ::ndk::ScopedAStatus destroy(int32_t in_instanceID) override
    {
        assert(in_instanceID < instances.size());
        instances[in_instanceID]->destroy();
        return ndk::ScopedAStatus::ok();
    }
};


#endif //ANDROIDAUDIOPLUGIN_AUDIOPLUGININTERFACEIMPL_H

} // namespace aap