#include <jni.h>
#include <android/log.h>
#include <android/binder_ibinder.h>
#include <android/binder_auto_utils.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <memory>

#include "aidl/org/androidaudioplugin/BnAudioPluginInterface.h"
#include "aidl/org/androidaudioplugin/BpAudioPluginInterface.h"
#include "aidl/org/androidaudioplugin/BnAudioPluginInterfaceCallback.h"
#include "aidl/org/androidaudioplugin/BpAudioPluginInterfaceCallback.h"
#include "aap/core/host/audio-plugin-host.h"
#include "aap/core/host/plugin-client-system.h"
#include "aap/core/host/android/audio-plugin-host-android.h"
#include "aap/core/android/android-application-context.h"
#include "AudioPluginInterfaceImpl.h"

#ifndef AAP_CORE_AUDIO_PLUGIN_HOST_ANDROID_H
#define AAP_CORE_AUDIO_PLUGIN_HOST_ANDROID_H

namespace aap {

class AndroidPluginClientSystem : public PluginClientSystem {
public:
    virtual inline ~AndroidPluginClientSystem() {}

    inline int32_t createSharedMemory(size_t size) override {
        return ASharedMemory_create(nullptr, size);
    }

    void ensurePluginServiceConnected(aap::PluginClientConnectionList* connections, std::string serviceName, std::function<void(std::string&)> callback) override;

    std::vector<std::string> getPluginPaths() override;

    inline void getAAPMetadataPaths(std::string path, std::vector<std::string> &results) override {
        // On Android there is no access to directories for each service. Only service identifier is passed.
        // Therefore, we simply add "path" which actually is a service identifier, to the results.
        results.emplace_back(path);
    }

    std::vector<PluginInformation *>
    getPluginsFromMetadataPaths(std::vector<std::string> &aapMetadataPaths) override;
};

class AndroidPluginClientConnectionData {
    class AudioPluginInterfaceCallbackImpl : public aidl::org::androidaudioplugin::BnAudioPluginInterfaceCallback {
            AndroidPluginClientConnectionData* owner;
        public:
            AudioPluginInterfaceCallbackImpl(AndroidPluginClientConnectionData* owner)
                : owner(owner) {
            }

            ~AudioPluginInterfaceCallbackImpl() {}

            ::ndk::ScopedAStatus requestProcess(int32_t in_instanceId) override {
                return owner->handleRequestProcess(in_instanceId);
            }

            ::ndk::ScopedAStatus hostExtension(int32_t in_instanceId,
                                               const std::string& in_uri,
                                               int32_t in_opcode,
                                               int32_t in_requestId,
                                               const std::shared_ptr<aidl::org::androidaudioplugin::IAudioPluginExtensionCallback>& in_callback) override {
                return owner->handleHostExtension(in_instanceId, in_uri, in_opcode, in_requestId, in_callback);
            }
        };

    ndk::SpAIBinder spAIBinder;
    std::shared_ptr<aidl::org::androidaudioplugin::IAudioPluginInterface> proxy;
    std::shared_ptr<AudioPluginInterfaceCallbackImpl> callback;
    ndk::ScopedAIBinder_DeathRecipient death_recipient;
    bool valid_{false};

    static void onBinderDied(void* cookie) {
        ((AndroidPluginClientConnectionData*) cookie)->handleBinderDeath();
    }

    static void onBinderUnlinked(void*) {
    }

    static void setDeathRecipientOnUnlinked(AIBinder_DeathRecipient* recipient) {
        if (!recipient)
            return;
        using SetOnUnlinked = void (*)(AIBinder_DeathRecipient*, void (*)(void*));
        auto setOnUnlinked = reinterpret_cast<SetOnUnlinked>(
                dlsym(RTLD_DEFAULT, "AIBinder_DeathRecipient_setOnUnlinked"));
        if (setOnUnlinked)
            setOnUnlinked(recipient, AndroidPluginClientConnectionData::onBinderUnlinked);
    }

    // Invoked on a binder thread when the plugin service process dies. Fails every in-flight
    // async AAPXS request (across all extensions, standard or not) on each remote instance so
    // callers stop waiting instead of hanging until timeout.
    void handleBinderDeath() {
        valid_ = false;
        for (auto& kv : remote_instances)
            if (kv.second)
                kv.second->abortAllPendingAAPXS("service disconnected");
    }
public:
    AndroidPluginClientConnectionData(AIBinder* aiBinder) {
        if (!aiBinder) {
            aap::a_log(AAP_LOG_LEVEL_ERROR, AAP_AIDL_SVC_LOG_TAG,
                       "AndroidPluginClientConnectionData: null binder");
            return;
        }
        spAIBinder.set(aiBinder);
        proxy = aidl::org::androidaudioplugin::BpAudioPluginInterface::fromBinder(spAIBinder);
        if (!proxy) {
            aap::a_log(AAP_LOG_LEVEL_ERROR, AAP_AIDL_SVC_LOG_TAG,
                       "AndroidPluginClientConnectionData: failed to create binder proxy");
            return;
        }
        callback = ndk::SharedRefBase::make<AudioPluginInterfaceCallbackImpl>(this);
        auto status = proxy->setCallback(callback->ref<AudioPluginInterfaceCallbackImpl>());
        if (!status.isOk()) {
            aap::a_log_f(AAP_LOG_LEVEL_ERROR, AAP_AIDL_SVC_LOG_TAG,
                         "AndroidPluginClientConnectionData: setCallback failed: %s",
                         status.getDescription().c_str());
            proxy.reset();
            callback.reset();
            return;
        }
        valid_ = true;

        // Fail in-flight async AAPXS requests promptly if the service process dies.
        death_recipient = ndk::ScopedAIBinder_DeathRecipient(
                AIBinder_DeathRecipient_new(AndroidPluginClientConnectionData::onBinderDied));
        setDeathRecipientOnUnlinked(death_recipient.get());
        auto deathStatus = AIBinder_linkToDeath(spAIBinder.get(), death_recipient.get(), this);
        if (deathStatus != STATUS_OK)
            aap::a_log_f(AAP_LOG_LEVEL_WARN, AAP_AIDL_SVC_LOG_TAG,
                         "AndroidPluginClientConnectionData: linkToDeath failed: %d", deathStatus);
    }
    virtual ~AndroidPluginClientConnectionData() {
        if (death_recipient.get() && spAIBinder.get())
            AIBinder_unlinkToDeath(spAIBinder.get(), death_recipient.get(), this);
    }

    bool isValid() const { return valid_; }

    std::function<::ndk::ScopedAStatus(int32_t instanceId)> request_process;
    std::map<int32_t, aap::RemotePluginInstance*> remote_instances;
    std::function<::ndk::ScopedAStatus(int32_t instanceId,
                                       const std::string& uri,
                                       int32_t opcode,
                                       int32_t requestId,
                                       const std::shared_ptr<aidl::org::androidaudioplugin::IAudioPluginExtensionCallback>& callback)> host_extension;
    std::function<::ndk::ScopedAStatus(::ndk::ScopedAParcel message)> handleMiscellaneousMessage;

    aidl::org::androidaudioplugin::IAudioPluginInterface *getProxy() { return proxy.get(); }

    ::ndk::ScopedAStatus handleRequestProcess(int32_t instanceId) {
        if (!request_process) {
            AAP_ASSERT_FALSE;
            return ::ndk::ScopedAStatus::fromServiceSpecificErrorWithMessage(1, "null request_process");
        }
        return request_process(instanceId);
    }

    ::ndk::ScopedAStatus handleHostExtension(int32_t instanceId,
                                             const std::string& uri,
                                             int32_t opcode,
                                             int32_t requestId,
                                             const std::shared_ptr<aidl::org::androidaudioplugin::IAudioPluginExtensionCallback>& completionCallback) {
        if (!host_extension) {
            AAP_ASSERT_FALSE;
            return ::ndk::ScopedAStatus::fromServiceSpecificErrorWithMessage(1, "null host_extension");
        }
        return host_extension(instanceId, uri, opcode, requestId, completionCallback);
    }

    void registerRemoteInstance(int32_t instanceId, aap::RemotePluginInstance* instance) {
        remote_instances[instanceId] = instance;
    }

    void unregisterRemoteInstance(int32_t instanceId) {
        remote_instances.erase(instanceId);
    }

    aap::RemotePluginInstance* getRemoteInstance(int32_t instanceId) {
        auto it = remote_instances.find(instanceId);
        return it != remote_instances.end() ? it->second : nullptr;
    }
};

} // namespace aap

#endif //AAP_CORE_AUDIO_PLUGIN_HOST_ANDROID_H
