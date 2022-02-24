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
#include "aap/core/host/audio-plugin-host.h"
#include "../core/shared-memory-extension.h"

namespace aap {

// This is a service class that is instantiated by a local Kotlin AudioPluginService instance.
// It is instantiated for one plugin per client.
// One client can instantiate multiple plugins.
class AudioPluginInterfaceImpl : public aidl::org::androidaudioplugin::BnAudioPluginInterface {
    std::unique_ptr<aap::PluginHostManager> manager;
    std::unique_ptr<aap::PluginHost> host;
    std::vector<AndroidAudioPluginBuffer> buffers{};

public:

    AudioPluginInterfaceImpl()
    {
        manager.reset(new PluginHostManager());
        host.reset(new PluginHost(manager.get()));
    }

    ::ndk::ScopedAStatus beginCreate(const std::string& in_pluginId, int32_t in_sampleRate, int32_t* _aidl_return) override
    {
        *_aidl_return = host->createInstance(in_pluginId, in_sampleRate);
        auto instance = host->getInstance(*_aidl_return);
        auto shm = new SharedMemoryExtension();
        shm->resizePortBuffer(instance->getPluginInformation()->getNumPorts());
        AndroidAudioPluginExtension ext;
        ext.uri = AAP_SHARED_MEMORY_EXTENSION_URI;
        ext.transmit_size = sizeof(SharedMemoryExtension);
        ext.data = shm;
        instance->addExtension(ext);
        buffers.resize(*_aidl_return + 1);
        auto & buffer = buffers[*_aidl_return];
        buffer.buffers = nullptr;
        buffer.num_buffers = 0;
        buffer.num_frames = 0;
        return ndk::ScopedAStatus::ok();
    }

    ::ndk::ScopedAStatus addExtension(int32_t in_instanceID, const std::string& in_uri, const ::ndk::ScopedFileDescriptor& in_sharedMemoryFD, int32_t in_size) override
    {
        if (in_instanceID < 0 || in_instanceID >= host->getInstanceCount())
            return ndk::ScopedAStatus::fromServiceSpecificErrorWithMessage(AAP_BINDER_ERROR_UNEXPECTED_INSTANCE_ID, "instance ID is out of range");
        AndroidAudioPluginExtension extension;
        extension.uri = in_uri.c_str();
        auto shmExt = getSharedMemoryExtension(host->getInstance(in_instanceID));
        if (shmExt == nullptr)
            return ndk::ScopedAStatus::fromServiceSpecificErrorWithMessage(AAP_BINDER_ERROR_SHARED_MEMORY_EXTENSION, "unable to get shared memory extension");
        auto fdRemote = in_sharedMemoryFD.get();
        auto dfd = fdRemote < 0 ? -1 : dup(fdRemote);
        shmExt->getExtensionFDs()->emplace_back(dfd);
        extension.transmit_size = in_size;
        extension.data = fdRemote < 0 ? nullptr : mmap(nullptr, in_size, PROT_READ | PROT_WRITE, MAP_SHARED, dfd, 0);
        host->getInstance(in_instanceID)->addExtension(extension);
        return ndk::ScopedAStatus::ok();
    }

    ::ndk::ScopedAStatus endCreate(int32_t in_instanceID) override
    {
        host->getInstance(in_instanceID)->completeInstantiation();
        return ndk::ScopedAStatus::ok();
    }

    ::ndk::ScopedAStatus isPluginAlive(int32_t in_instanceID, bool* _aidl_return) override
    {
        if (in_instanceID < 0 || in_instanceID >= host->getInstanceCount())
            return ndk::ScopedAStatus::fromServiceSpecificErrorWithMessage(AAP_BINDER_ERROR_UNEXPECTED_INSTANCE_ID, "instance ID is out of range");
        *_aidl_return = host->getInstance(in_instanceID) != nullptr;
        return ndk::ScopedAStatus::ok();
    }

    // Since AIDL does not support sending List of ParcelFileDescriptor it is sent one by one.
    // Here we just cache the FDs, and process them later at prepare().
    ::ndk::ScopedAStatus prepareMemory(int32_t in_instanceID, int32_t in_shmFDIndex, const ::ndk::ScopedFileDescriptor& in_sharedMemoryFD) override
    {
        if (in_instanceID < 0 || in_instanceID >= host->getInstanceCount())
            return ndk::ScopedAStatus::fromServiceSpecificErrorWithMessage(AAP_BINDER_ERROR_UNEXPECTED_INSTANCE_ID, "instance ID is out of range");
        auto shmExt = getSharedMemoryExtension(host->getInstance(in_instanceID));
        if (shmExt == nullptr)
            return ndk::ScopedAStatus::fromServiceSpecificErrorWithMessage(AAP_BINDER_ERROR_SHARED_MEMORY_EXTENSION, "unable to get shared memory extension");
        auto fdRemote = in_sharedMemoryFD.get();
        if (fdRemote < 0)
            return ndk::ScopedAStatus::fromServiceSpecificErrorWithMessage(AAP_BINDER_ERROR_INVALID_SHARED_MEMORY_FD, "invalid shared memory fd was passed");
        auto dfd = dup(fdRemote);
        shmExt->setPortBufferFD(in_shmFDIndex, dfd);
        return ndk::ScopedAStatus::ok();
    }

    ::ndk::ScopedAStatus prepare(int32_t in_instanceID, int32_t in_frameCount, int32_t in_portCount) override
    {
        if (in_instanceID < 0 || in_instanceID >= host->getInstanceCount())
            return ndk::ScopedAStatus::fromServiceSpecificErrorWithMessage(AAP_BINDER_ERROR_UNEXPECTED_INSTANCE_ID, "instance ID is out of range");
        auto instance = host->getInstance(in_instanceID);
        auto shmExt = getSharedMemoryExtension(instance);
        if (shmExt == nullptr)
            return ndk::ScopedAStatus::fromServiceSpecificErrorWithMessage(AAP_BINDER_ERROR_SHARED_MEMORY_EXTENSION, "unable to get shared memory extension");
        shmExt->completeInitialization(in_frameCount);
        int ret = prepare(host->getInstance(in_instanceID), buffers[in_instanceID], in_frameCount, in_portCount);
        if (ret != 0)
            return ndk::ScopedAStatus::fromServiceSpecificErrorWithMessage(AAP_BINDER_ERROR_SHARED_MEMORY_EXTENSION, "unable to get shared memory extension");
        return ndk::ScopedAStatus::ok();
    }

    void freeBuffers(PluginInstance* instance, AndroidAudioPluginBuffer& buffer)
    {
        if (buffer.buffers)
            for (int i = 0; i < buffer.num_buffers; i++)
                if (buffer.buffers[i])
                    munmap(buffer.buffers[i], buffer.num_buffers * sizeof(float));
    }

    int prepare(PluginInstance* instance, AndroidAudioPluginBuffer& buffer, int32_t frameCount, int32_t portCount)
    {
        int ret = resetBuffers(instance, buffer, frameCount);
        if (ret != 0) {
            __android_log_print(ANDROID_LOG_ERROR, "AndroidAudioPlugin", "Failed to prepare shared memory buffers.");
            return ret;
        }
        instance->prepare(frameCount, &buffer);
        return 0;
    }

    int resetBuffers(PluginInstance* instance, AndroidAudioPluginBuffer& buffer, int frameCount)
    {
        int nPorts = instance->getPluginInformation()->getNumPorts();
        auto shmExt = getSharedMemoryExtension(instance);
        if (shmExt == nullptr)
            return AAP_BINDER_ERROR_SHARED_MEMORY_EXTENSION;
        shmExt->resizePortBuffer(nPorts);
        if (buffer.num_buffers != nPorts)
            freeBuffers(instance, buffer);
        shmExt->resizePortBuffer(nPorts);

        buffer.num_buffers = nPorts;
        buffer.num_frames = frameCount;
        int n = buffer.num_buffers;
        if (buffer.buffers == nullptr)
            buffer.buffers = (void **) calloc(sizeof(void *), n);
        for (int i = 0; i < n; i++) {
            if (buffer.buffers[i])
                munmap(buffer.buffers[i], buffer.num_frames * sizeof(float));
            buffer.buffers[i] = mmap(nullptr, buffer.num_frames * sizeof(float), PROT_READ | PROT_WRITE,
                                     MAP_SHARED, shmExt->getPortBufferFD(i), 0);
            if (buffer.buffers[i] == MAP_FAILED) {
                int err = AAP_BINDER_ERROR_MMAP_FAILED;
                __android_log_print(ANDROID_LOG_ERROR, "AndroidAudioPlugin",
                                    "mmap failed at buffer %d. errno = %d", i, err);
                return err;
            }
            if (buffer.buffers[i] == nullptr) {
                __android_log_print(ANDROID_LOG_ERROR, "AndroidAudioPlugin",
                                    "mmap returned null pointer");
                return AAP_BINDER_ERROR_MMAP_NULL_RETURN;
            }
        }
        return 0;
    }

    ::ndk::ScopedAStatus activate(int32_t in_instanceID) override
    {
        if (in_instanceID < 0 || in_instanceID >= host->getInstanceCount())
            return ndk::ScopedAStatus::fromServiceSpecificErrorWithMessage(AAP_BINDER_ERROR_UNEXPECTED_INSTANCE_ID, "instance ID is out of range");
        host->getInstance(in_instanceID)->activate();
        return ndk::ScopedAStatus::ok();
    }

    ::ndk::ScopedAStatus process(int32_t in_instanceID, int32_t in_timeoutInNanoseconds) override
    {
        if (in_instanceID < 0 || in_instanceID >= host->getInstanceCount())
            return ndk::ScopedAStatus::fromServiceSpecificErrorWithMessage(AAP_BINDER_ERROR_UNEXPECTED_INSTANCE_ID, "instance ID is out of range");
        host->getInstance(in_instanceID)->process(&buffers[in_instanceID], in_timeoutInNanoseconds);
        return ndk::ScopedAStatus::ok();
    }

    ::ndk::ScopedAStatus deactivate(int32_t in_instanceID) override
    {
        if (in_instanceID < 0 || in_instanceID >= host->getInstanceCount())
            return ndk::ScopedAStatus::fromServiceSpecificErrorWithMessage(AAP_BINDER_ERROR_UNEXPECTED_INSTANCE_ID, "instance ID is out of range");
        host->getInstance(in_instanceID)->deactivate();
        return ndk::ScopedAStatus::ok();
    }

    ::ndk::ScopedAStatus getStateSize(int32_t in_instanceID, int32_t *_aidl_return) override
    {
        if (in_instanceID < 0 || in_instanceID >= host->getInstanceCount())
            return ndk::ScopedAStatus::fromServiceSpecificErrorWithMessage(AAP_BINDER_ERROR_UNEXPECTED_INSTANCE_ID, "instance ID is out of range");
        *_aidl_return = host->getInstance(in_instanceID)->getStateSize();
        return ndk::ScopedAStatus::ok();
    }

    ::ndk::ScopedAStatus getState(int32_t in_instanceID, const ::ndk::ScopedFileDescriptor& in_sharedMemoryFD) override
    {
        if (in_instanceID < 0 || in_instanceID >= host->getInstanceCount())
            return ndk::ScopedAStatus::fromServiceSpecificErrorWithMessage(AAP_BINDER_ERROR_UNEXPECTED_INSTANCE_ID, "instance ID is out of range");
        auto instance = host->getInstance(in_instanceID);
        auto state = instance->getState();
        auto fdRemote = in_sharedMemoryFD.get();
        if (fdRemote < 0)
            return ndk::ScopedAStatus::fromServiceSpecificErrorWithMessage(AAP_BINDER_ERROR_INVALID_SHARED_MEMORY_FD, "invalid shared memory fd was passed");
        auto dfd = dup(fdRemote);
        auto dst = mmap(nullptr, state.data_size, PROT_READ | PROT_WRITE, MAP_SHARED, dfd, 0);
        memcpy(dst, state.raw_data, state.data_size);
        munmap(dst, state.data_size);
        return ndk::ScopedAStatus::ok();
    }

    ::ndk::ScopedAStatus setState(int32_t in_instanceID, const ::ndk::ScopedFileDescriptor& in_sharedMemoryFD, int32_t in_size) override
    {
        if (in_instanceID < 0 || in_instanceID >= host->getInstanceCount())
            return ndk::ScopedAStatus::fromServiceSpecificErrorWithMessage(AAP_BINDER_ERROR_UNEXPECTED_INSTANCE_ID, "instance ID is out of range");
        auto instance = host->getInstance(in_instanceID);
        auto fdRemote = in_sharedMemoryFD.get();
        if (fdRemote < 0)
            return ndk::ScopedAStatus::fromServiceSpecificErrorWithMessage(AAP_BINDER_ERROR_INVALID_SHARED_MEMORY_FD, "invalid shared memory fd was passed");
        auto dfd = dup(fdRemote);
        auto src = mmap(nullptr, in_size, PROT_READ | PROT_WRITE, MAP_SHARED, dfd, 0);
        instance->setState(src, in_size);
        munmap(src, in_size);
        return ndk::ScopedAStatus::ok();
    }

    ::ndk::ScopedAStatus destroy(int32_t in_instanceID) override
    {
        if (in_instanceID < 0 || in_instanceID >= host->getInstanceCount())
            return ndk::ScopedAStatus::fromServiceSpecificErrorWithMessage(AAP_BINDER_ERROR_UNEXPECTED_INSTANCE_ID, "instance ID is out of range");
        host->destroyInstance(host->getInstance(in_instanceID));
        return ndk::ScopedAStatus::ok();
    }
};


#endif //ANDROIDAUDIOPLUGIN_AUDIOPLUGININTERFACEIMPL_H

} // namespace aap