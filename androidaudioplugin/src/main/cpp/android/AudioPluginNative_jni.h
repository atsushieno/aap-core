
#ifndef AUDIOPLUGINNATIVE_JNI_H_INCLUDED
#define AUDIOPLUGINNATIVE_JNI_H_INCLUDED

#include <jni.h>
#include <aap/core/plugin-information.h>
#include <aap/core/host/audio-plugin-host.h>
#include <aap/core/aapxs/extension-service.h>

extern "C" {

aap::PluginInformation *
pluginInformation_fromJava(JNIEnv *env, jobject pluginInformation);

jobjectArray queryInstalledPluginsJNI();

void ensureServiceConnectedFromJni(jint connectorInstanceId, std::string servicePackageName,
                                   std::function<void(std::string&)> callback);

int32_t getConnectorInstanceId(aap::PluginClientConnectionList *connections);

} // extern "C"

class JNIClientAAPXSManager : public aap::AAPXSClientInstanceManager {
    AndroidAudioPlugin* getPlugin() override;
    AAPXSFeature* getExtensionFeature(const char* uri) override;
    const aap::PluginInformation* getPluginInformation() override;
    AAPXSClientInstance* setupAAPXSInstance(AAPXSFeature *feature, int32_t dataSize = -1) override;

};

#endif // AUDIOPLUGINNATIVE_JNI_H_INCLUDED