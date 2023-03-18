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
#include "aap/core/host/shared-memory-store.h"
#include "../core/hosting/plugin-service-list.h"

#define AAP_AIDL_SVC_LOG_TAG "AAP.aidl.svc"

namespace aap {

// This is a service class that is instantiated by a local Kotlin AudioPluginService instance.
// It is instantiated for one plugin per client.
// One client can instantiate multiple plugins.
class AudioPluginInterfaceImpl : public aidl::org::androidaudioplugin::BnAudioPluginInterface {
    PluginListSnapshot plugins;
    std::unique_ptr<PluginService> svc;
    std::vector<aap_buffer_t> buffers{};
    std::shared_ptr<aidl::org::androidaudioplugin::IAudioPluginInterfaceCallback> callback{nullptr};

public:

    AudioPluginInterfaceImpl() {
        plugins = PluginListSnapshot::queryServices();
        svc.reset(new PluginService(&plugins, [&](int32_t instanceId) {
            if (callback)
                callback->requestProcess(instanceId);
        }));
        aap::PluginServiceList::getInstance()->addBoundServiceInProcess(svc.get());
    }

    ~AudioPluginInterfaceImpl() {
        aap::PluginServiceList::getInstance()->removeBoundServiceInProcess(svc.get());
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
                auto shmExt = instance->getSharedMemoryStore();
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

        auto shmExt = instance->getSharedMemoryStore();
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

        auto shm = instance->getSharedMemoryStore();
        shm->resizePortBufferByCount(instance->getNumPorts());
        return ndk::ScopedAStatus::ok();
    }

    ::ndk::ScopedAStatus
    endPrepare(int32_t in_instanceID, int32_t in_frameCount) override {
        auto instance = svc->getLocalInstance(in_instanceID);
        CHECK_INSTANCE(instance, in_instanceID)

        auto shmExt = dynamic_cast<ServicePluginSharedMemoryStore*>(instance->getSharedMemoryStore());
        if (shmExt == nullptr)
            return ndk::ScopedAStatus::fromServiceSpecificErrorWithMessage(
                    AAP_BINDER_ERROR_SHARED_MEMORY_EXTENSION,
                    "unable to get shared memory extension");
        if (!shmExt->completeServiceInitialization(in_frameCount, *instance, DEFAULT_CONTROL_BUFFER_SIZE))
            return ndk::ScopedAStatus::fromServiceSpecificErrorWithMessage(
                    AAP_BINDER_ERROR_SHARED_MEMORY_EXTENSION,
                    "failed to allocate shared memory");
        instance->prepare(in_frameCount);
        return ndk::ScopedAStatus::ok();
    }

    ::ndk::ScopedAStatus activate(int32_t in_instanceID) override {
        auto instance = svc->getLocalInstance(in_instanceID);
        CHECK_INSTANCE(instance, in_instanceID)

        instance->activate();
        return ndk::ScopedAStatus::ok();
    }

    ::ndk::ScopedAStatus process(int32_t in_instanceID, int32_t in_frameCount, int32_t in_timeoutInNanoseconds) override {
        auto instance = svc->getLocalInstance(in_instanceID);
        CHECK_INSTANCE(instance, in_instanceID)

        instance->process(in_frameCount, in_timeoutInNanoseconds);
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
