#ifndef ANDROIDAUDIOPLUGIN_AUDIOPLUGININTERFACEIMPL_H
#define ANDROIDAUDIOPLUGIN_AUDIOPLUGININTERFACEIMPL_H

#include <sstream>
#include <android/sharedmem.h>
#include <android/binder_ibinder.h>
#include <android/binder_ibinder_jni.h>
#include <android/binder_interface_utils.h>
#include <android/binder_parcel.h>
#include <android/binder_parcel_utils.h>
#include <android/binder_status.h>
#include <android/binder_auto_utils.h>
#include "aap/core/host/audio-plugin-host.h"
#include "../core/hosting/shared-memory-store.h"

#define AAP_AIDL_SVC_LOG_TAG "AAP.aidl.svc"

namespace aap {

// This is a service class that is instantiated by a local Kotlin AudioPluginService instance.
// It is instantiated for one plugin per client.
// One client can instantiate multiple plugins.
class AudioPluginInterfaceImpl : public aidl::org::androidaudioplugin::BnAudioPluginInterface {
    PluginListSnapshot plugins;
    std::unique_ptr<PluginService> svc;
    std::vector<AndroidAudioPluginBuffer> buffers{};
    std::shared_ptr<aidl::org::androidaudioplugin::IAudioPluginInterfaceCallback> callback{nullptr};

public:

    AudioPluginInterfaceImpl() {
        plugins = PluginListSnapshot::queryServices();
        svc.reset(new PluginService(&plugins));
    }

    ::ndk::ScopedAStatus setCallback(const std::shared_ptr<aidl::org::androidaudioplugin::IAudioPluginInterfaceCallback>& in_callback) override {
        if (callback.get()) {
            return ndk::ScopedAStatus::fromServiceSpecificErrorWithMessage(
                    AAP_BINDER_ERROR_CALLBACK_ALREADY_SET, "failed to create AAP service instance.");
        }
        callback.reset(in_callback.get());

        return ndk::ScopedAStatus::ok();
    }

    ::ndk::ScopedAStatus beginCreate(const std::string &in_pluginId, int32_t in_sampleRate,
                                     int32_t *_aidl_return) override {
        *_aidl_return = svc->createInstance(in_pluginId, in_sampleRate);
        if (*_aidl_return < 0)
        return ndk::ScopedAStatus::fromServiceSpecificErrorWithMessage(
                AAP_BINDER_ERROR_CREATE_INSTANCE_FAILED, "failed to create AAP service instance.");

        buffers.resize(*_aidl_return + 1);
        auto &buffer = buffers[*_aidl_return];
        buffer.buffers = nullptr;
        buffer.num_buffers = 0;
        buffer.num_frames = 0;
        return ndk::ScopedAStatus::ok();
    }

#define CHECK_INSTANCE(_instance_, _id_) { \
    if (_instance_ == nullptr) { \
        std::stringstream msg; \
        msg << "The specified instance " << _id_ << " does not exist."; \
        aap::a_log_f(AAP_LOG_LEVEL_ERROR, AAP_AIDL_SVC_LOG_TAG, msg.str().c_str()); \
        return ndk::ScopedAStatus::fromServiceSpecificErrorWithMessage( \
                AAP_BINDER_ERROR_UNEXPECTED_INSTANCE_ID, msg.str().c_str()); \
    } \
}

    ::ndk::ScopedAStatus addExtension(int32_t in_instanceID, const std::string &in_uri,
                                      const ::ndk::ScopedFileDescriptor &in_sharedMemoryFD,
                                      int32_t in_size) override {
        auto feature = svc->getExtensionFeature(in_uri.c_str());
        if (feature == nullptr) {
            return ndk::ScopedAStatus::fromServiceSpecificErrorWithMessage(
                    AAP_BINDER_ERROR_UNEXPECTED_FEATURE_URI, (std::string{"The host requested plugin extension '"} + in_uri + "', but this plugin service does not support it.").c_str());
        } else {
            auto instance = svc->getLocalInstance(in_instanceID);
            CHECK_INSTANCE(instance, in_instanceID)

            auto aapxsInstance = instance->setupAAPXSInstance(feature, in_size);
            aapxsInstance->plugin_instance_id = in_instanceID;
            if (in_size > 0) {
                auto shmExt = instance->getAAPXSSharedMemoryStore();
                assert(shmExt != nullptr);
                auto fdRemote = in_sharedMemoryFD.get();
                auto dfd = fdRemote < 0 ? -1 : dup(fdRemote);
                aapxsInstance->data = shmExt->addExtensionFD(dfd, in_size);
            }
        }
        return ndk::ScopedAStatus::ok();
    }

    ::ndk::ScopedAStatus endCreate(int32_t in_instanceID) override {
        auto instance = svc->getLocalInstance(in_instanceID);
        CHECK_INSTANCE(instance, in_instanceID)

        instance->completeInstantiation();
        instance->startPortConfiguration();
        return ndk::ScopedAStatus::ok();
    }

    ::ndk::ScopedAStatus isPluginAlive(int32_t in_instanceID, bool *_aidl_return) override {
        auto instance = svc->getLocalInstance(in_instanceID);
        *_aidl_return = instance != nullptr;
        return ndk::ScopedAStatus::ok();
    }

    // Since AIDL does not support sending List of ParcelFileDescriptor it is sent one by one.
    // Here we just cache the FDs, and process them later at prepare().
    ::ndk::ScopedAStatus prepareMemory(int32_t in_instanceID, int32_t in_shmFDIndex,
                                       const ::ndk::ScopedFileDescriptor &in_sharedMemoryFD) override {
        auto instance = svc->getLocalInstance(in_instanceID);
        CHECK_INSTANCE(instance, in_instanceID)

        auto shmExt = instance->getAAPXSSharedMemoryStore();
        if (shmExt == nullptr)
            return ndk::ScopedAStatus::fromServiceSpecificErrorWithMessage(
                    AAP_BINDER_ERROR_SHARED_MEMORY_EXTENSION,
                    "unable to get shared memory extension");
        auto fdRemote = in_sharedMemoryFD.get();
        if (fdRemote < 0)
            return ndk::ScopedAStatus::fromServiceSpecificErrorWithMessage(
                    AAP_BINDER_ERROR_INVALID_SHARED_MEMORY_FD,
                    "invalid shared memory fd was passed");
        auto dfd = dup(fdRemote);
        shmExt->setPortBufferFD(in_shmFDIndex, dfd);
        return ndk::ScopedAStatus::ok();
    }

    ::ndk::ScopedAStatus
    beginPrepare(int32_t in_instanceID) override {
        auto instance = svc->getLocalInstance(in_instanceID);
        CHECK_INSTANCE(instance, in_instanceID)

        instance->confirmPorts();
        instance->scanParametersAndBuildList();

        auto shm = instance->getAAPXSSharedMemoryStore();
        shm->resizePortBufferByCount(instance->getNumPorts());
        return ndk::ScopedAStatus::ok();
    }

    ::ndk::ScopedAStatus
    endPrepare(int32_t in_instanceID, int32_t in_frameCount) override {
        auto instance = svc->getLocalInstance(in_instanceID);
        CHECK_INSTANCE(instance, in_instanceID)

        auto shmExt = instance->getAAPXSSharedMemoryStore();
        if (shmExt == nullptr)
            return ndk::ScopedAStatus::fromServiceSpecificErrorWithMessage(
                    AAP_BINDER_ERROR_SHARED_MEMORY_EXTENSION,
                    "unable to get shared memory extension");
        if (!shmExt->completeServiceInitialization(in_frameCount))
            return ndk::ScopedAStatus::fromServiceSpecificErrorWithMessage(
                    AAP_BINDER_ERROR_SHARED_MEMORY_EXTENSION,
                    "failed to allocate shared memory");
        int ret = prepare(instance, buffers[in_instanceID], in_frameCount);
        if (ret != 0)
            return ndk::ScopedAStatus::fromServiceSpecificErrorWithMessage(
                    AAP_BINDER_ERROR_SHARED_MEMORY_EXTENSION,
                    "unable to get shared memory extension");
        return ndk::ScopedAStatus::ok();
    }

    void freeBuffers(LocalPluginInstance *instance, AndroidAudioPluginBuffer &buffer) {
        if (buffer.buffers)
            for (int i = 0; i < buffer.num_buffers; i++)
                if (buffer.buffers[i])
                    munmap(buffer.buffers[i], buffer.num_buffers * sizeof(float));
    }

    int prepare(LocalPluginInstance *instance, AndroidAudioPluginBuffer &buffer, int32_t frameCount) {
        int ret = resetBuffers(instance, buffer, frameCount);
        if (ret != 0) {
            __android_log_print(ANDROID_LOG_ERROR, "AndroidAudioPlugin",
                                "Failed to prepare shared memory buffers.");
            return ret;
        }
        instance->prepare(frameCount, &buffer);
        return 0;
    }

    int resetBuffers(LocalPluginInstance *instance, AndroidAudioPluginBuffer &buffer, int frameCount) {
        int nPorts = instance->getNumPorts();
        auto shmExt = instance->getAAPXSSharedMemoryStore();
        if (shmExt == nullptr)
            return AAP_BINDER_ERROR_SHARED_MEMORY_EXTENSION;
        // FIXME: duplicate resize?
        shmExt->resizePortBufferByCount(nPorts);
        if (buffer.num_buffers != nPorts)
            freeBuffers(instance, buffer);
        shmExt->resizePortBufferByCount(nPorts);

        buffer.num_buffers = nPorts;
        buffer.num_frames = frameCount;
        int n = buffer.num_buffers;
        if (buffer.buffers == nullptr)
            buffer.buffers = (void **) calloc(sizeof(void *), n);
        for (int i = 0; i < n; i++) {
            if (buffer.buffers[i])
                munmap(buffer.buffers[i], buffer.num_frames * sizeof(float));
            buffer.buffers[i] = mmap(nullptr, buffer.num_frames * sizeof(float),
                                     PROT_READ | PROT_WRITE,
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

    ::ndk::ScopedAStatus activate(int32_t in_instanceID) override {
        auto instance = svc->getLocalInstance(in_instanceID);
        CHECK_INSTANCE(instance, in_instanceID)

        instance->activate();
        return ndk::ScopedAStatus::ok();
    }

    ::ndk::ScopedAStatus process(int32_t in_instanceID, int32_t in_timeoutInNanoseconds) override {
        auto instance = svc->getLocalInstance(in_instanceID);
        CHECK_INSTANCE(instance, in_instanceID)

        instance->process(&buffers[in_instanceID], in_timeoutInNanoseconds);
        return ndk::ScopedAStatus::ok();
    }

    ::ndk::ScopedAStatus deactivate(int32_t in_instanceID) override {
        auto instance = svc->getLocalInstance(in_instanceID);
        CHECK_INSTANCE(instance, in_instanceID)

        instance->deactivate();
        return ndk::ScopedAStatus::ok();
    }

    ::ndk::ScopedAStatus extension(int32_t in_instanceID, const std::string& in_uri, int32_t in_size) override {
        auto instance = svc->getLocalInstance(in_instanceID);
        CHECK_INSTANCE(instance, in_instanceID)

        instance->controlExtension(in_uri, in_size);
        return ndk::ScopedAStatus::ok();
    }

    ::ndk::ScopedAStatus destroy(int32_t in_instanceID) override {
        auto instance = svc->getLocalInstance(in_instanceID);
        CHECK_INSTANCE(instance, in_instanceID)

        svc->destroyInstance(instance);
        return ndk::ScopedAStatus::ok();
    }
};


} // namespace aap

#endif //ANDROIDAUDIOPLUGIN_AUDIOPLUGININTERFACEIMPL_H
