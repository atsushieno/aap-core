#include <jni.h>
#include <aap/core/host/audio-plugin-host.h>

extern "C" {

aap::PluginInformation *
pluginInformation_fromJava(JNIEnv *env, jobject pluginInformation);

jobjectArray queryInstalledPluginsJNI();

void ensureServiceConnectedFromJni(jint connectorInstanceId, std::string servicePackageName,
                                   std::function<void(std::string)> callback);

int32_t getConnectorInstanceId(aap::PluginClientConnectionList *connections);

} // namespace aap
