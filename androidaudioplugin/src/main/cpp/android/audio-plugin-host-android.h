#include <jni.h>
#include <android/log.h>
#include <stdlib.h>
#include <sys/mman.h>

#include <memory>
#include "aidl/org/androidaudioplugin/BnAudioPluginInterface.h"
#include "aidl/org/androidaudioplugin/BpAudioPluginInterface.h"
#include "aap/core/host/audio-plugin-host.h"
#include "aap/core/host/android/audio-plugin-host-android.h"
#include "aap/core/host/android/android-application-context.h"
#include "AudioPluginInterfaceImpl.h"

#ifndef AAP_CORE_AUDIO_PLUGIN_HOST_ANDROID_H
#define AAP_CORE_AUDIO_PLUGIN_HOST_ANDROID_H

#define AAP_BINDER_EXTENSION_URI "urn:aap:internals:ai_binder_provider_extension"

namespace aap {

class AndroidPluginHostPAL : public PluginHostPAL {
public:
    virtual inline ~AndroidPluginHostPAL() {}

    inline int32_t createSharedMemory(size_t size) override {
        return ASharedMemory_create(nullptr, size);
    }

    std::vector<std::string> getPluginPaths() override;

    inline void getAAPMetadataPaths(std::string path, std::vector<std::string> &results) override {
        // On Android there is no access to directories for each service. Only service identifier is passed.
        // Therefore, we simply add "path" which actually is a service identifier, to the results.
        results.emplace_back(path);
    }

    std::vector<PluginInformation *>
    getPluginsFromMetadataPaths(std::vector<std::string> &aapMetadataPaths) override;

    void initializeKnownPlugins(jobjectArray jPluginInfos = nullptr);
};

} // namespace aap

#endif //AAP_CORE_AUDIO_PLUGIN_HOST_ANDROID_H
