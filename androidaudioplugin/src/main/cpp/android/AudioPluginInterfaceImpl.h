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

class AndroidAudioPluginServiceCallback : public AudioPluginServiceCallback {
public:
    AndroidAudioPluginServiceCallback() {}
    virtual ~AndroidAudioPluginServiceCallback() {}

    std::shared_ptr<aidl::org::androidaudioplugin::IAudioPluginInterfaceCallback> callback_proxy{nullptr};

    void hostExtension(int32_t in_instanceId, const std::string& in_uri, int32_t in_opcode) override {
        callback_proxy->hostExtension(in_instanceId, in_uri, in_opcode);
    }
    void requestProcess(int32_t in_instanceId) override {
        callback_proxy->requestProcess(in_instanceId);
    }
};

// This is a service class that is instantiated by a local Kotlin AudioPluginService instance.
// It is instantiated for one plugin per client.
// One client can instantiate multiple plugins.
class AudioPluginInterfaceImpl : public aidl::org::androidaudioplugin::BnAudioPluginInterface {
    PluginListSnapshot plugins;
    std::unique_ptr<PluginService> svc;
    std::vector<aap_buffer_t> buffers{};
    std::unique_ptr<AndroidAudioPluginServiceCallback> plugin_service_callback{nullptr};

public:

    AudioPluginInterfaceImpl() {
        plugins = PluginListSnapshot::queryServices();
        plugin_service_callback = std::make_unique<AndroidAudioPluginServiceCallback>();
        svc.reset(new PluginService(&plugins, plugin_service_callback.get()));
        aap::PluginServiceList::getInstance()->addBoundServiceInProcess(svc.get());
    }

    ~AudioPluginInterfaceImpl() {
        aap::PluginServiceList::getInstance()->removeBoundServiceInProcess(svc.get());
    }

    ::ndk::ScopedAStatus setCallback(const std::shared_ptr<aidl::org::androidaudioplugin::IAudioPluginInterfaceCallback>& in_callback) override {
        if (plugin_service_callback->callback_proxy.get()) {
            return ndk::ScopedAStatus::fromServiceSpecificErrorWithMessage(
                    AAP_BINDER_ERROR_CALLBACK_ALREADY_SET, "failed to create AAP service instance: callback already exists.");
        }
        plugin_service_callback->callback_proxy.reset(in_callback.get());

        return ndk::ScopedAStatus::ok();
    }

    static void aapxs_host_ipc_sender_func(void* context,
                                           const char* uri,
                                           int32_t instanceId,
                                           int32_t opcode) {
        ((AudioPluginInterfaceImpl*) context)->plugin_service_callback->hostExtension(instanceId, uri, opcode);
    }

    ::ndk::ScopedAStatus beginCreate(const std::string &in_pluginId, int32_t in_sampleRate,
                                     int32_t *_aidl_return) override {
        *_aidl_return = svc->createInstance(in_pluginId, in_sampleRate);
        auto instance = svc->getLocalInstance(*_aidl_return);
        instance->setIpcExtensionMessageSender(aapxs_host_ipc_sender_func, this);
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
        auto instance = svc->getLocalInstance(in_instanceID);
        CHECK_INSTANCE(instance, in_instanceID)
        if (in_size > 0) {
            auto shmExt = instance->getSharedMemoryStore();
            assert(shmExt != nullptr);
            auto fdRemote = in_sharedMemoryFD.get();
            auto dfd = fdRemote < 0 ? -1 : dup(fdRemote);
            shmExt->addExtensionFD(dfd, in_size);
            shmExt->getExtensionUriToIndexMap()[in_uri] = shmExt->getExtensionBufferCount() - 1;
        }
        return ndk::ScopedAStatus::ok();
    }

    ::ndk::ScopedAStatus endCreate(int32_t in_instanceID) override {
        auto instance = svc->getLocalInstance(in_instanceID);
        CHECK_INSTANCE(instance, in_instanceID)

        instance->completeInstantiation();
        instance->setupAAPXS();
        instance->setupAAPXSInstances();
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

    ::ndk::ScopedAStatus extension(int32_t in_instanceID, const std::string& in_uri, int32_t in_opcode) override {
        auto instance = svc->getLocalInstance(in_instanceID);
        CHECK_INSTANCE(instance, in_instanceID)

        instance->controlExtension(in_uri, in_opcode, 0); // no requestId is assigned for synchronous Binder calls.
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
