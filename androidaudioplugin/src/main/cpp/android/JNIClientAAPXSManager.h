
#ifndef AUDIOPLUGINNATIVE_JNI_H_INCLUDED
#define AUDIOPLUGINNATIVE_JNI_H_INCLUDED

#include <jni.h>
#include <aap/core/plugin-information.h>
#include <aap/core/host/audio-plugin-host.h>
#include <aap/core/aapxs/extension-service.h>

// FIXME: this should be translated into native testing (when we get some testing foundation!)
class JNIClientAAPXSManager : public aap::AAPXSClientInstanceManager {
    AndroidAudioPlugin* getPlugin() override { return nullptr; }
    AAPXSFeature* getExtensionFeature(const char* uri) override { return nullptr; }
    const aap::PluginInformation* getPluginInformation() override { return nullptr; }
    AAPXSClientInstance* setupAAPXSInstance(AAPXSFeature *feature, int32_t dataSize = -1) override { return nullptr; }

};

#endif // AUDIOPLUGINNATIVE_JNI_H_INCLUDED