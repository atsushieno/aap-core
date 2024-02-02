#include <jni.h>
#include <android/log.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <memory>

#include "aidl/org/androidaudioplugin/BnAudioPluginInterface.h"
#include "aidl/org/androidaudioplugin/BpAudioPluginInterface.h"
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
    ndk::SpAIBinder spAIBinder;
    std::shared_ptr<aidl::org::androidaudioplugin::IAudioPluginInterface> proxy;
    std::shared_ptr<aidl::org::androidaudioplugin::IAudioPluginInterfaceCallback> callback;

    class AudioPluginCallback : public aidl::org::androidaudioplugin::IAudioPluginInterfaceCallbackDefault {
        AndroidPluginClientConnectionData* owner;

    public:
        AudioPluginCallback(AndroidPluginClientConnectionData* owner) : owner(owner) {}

        ::ndk::ScopedAStatus requestProcess(int32_t in_instanceId) override {
            return owner->handleRequestProcess(in_instanceId);
        }

        ::ndk::ScopedAStatus hostExtension(int32_t in_instanceId, const std::string& in_uri, int32_t in_opcode) override {
            return owner->handleHostExtension(in_instanceId, in_uri, in_opcode);
        }
    };

public:
    AndroidPluginClientConnectionData(AIBinder* aiBinder) {
        spAIBinder.set(aiBinder);
        proxy = aidl::org::androidaudioplugin::BpAudioPluginInterface::fromBinder(spAIBinder);
        callback = std::make_shared<AudioPluginCallback>(this);
        proxy->setCallback(callback);
    }

    std::function<::ndk::ScopedAStatus(int32_t instanceId)> request_process;
    std::function<::ndk::ScopedAStatus(int32_t instanceId, const std::string& uri, int32_t opcode)> host_extension;
    std::function<::ndk::ScopedAStatus(::ndk::ScopedAParcel message)> handleMiscellaneousMessage;

    aidl::org::androidaudioplugin::IAudioPluginInterface *getProxy() { return proxy.get(); }

    ::ndk::ScopedAStatus handleRequestProcess(int32_t instanceId) {
        if (!request_process) {
            AAP_ASSERT_FALSE;
            return ::ndk::ScopedAStatus::fromServiceSpecificErrorWithMessage(1, "null request_process");
        }
        return request_process(instanceId);
    }

    ::ndk::ScopedAStatus handleHostExtension(int32_t instanceId, const std::string& uri, int32_t opcode) {
        if (!host_extension) {
            AAP_ASSERT_FALSE;
            return ::ndk::ScopedAStatus::fromServiceSpecificErrorWithMessage(1, "null host_extension");
        }
        return host_extension(instanceId, uri, opcode);
    }
};

} // namespace aap

#endif //AAP_CORE_AUDIO_PLUGIN_HOST_ANDROID_H
