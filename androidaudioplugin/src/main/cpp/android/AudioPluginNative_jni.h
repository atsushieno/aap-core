#include <jni.h>
#include <aap/core/host/audio-plugin-host.h>

extern "C" {

aap::PluginInformation *
pluginInformation_fromJava(JNIEnv *env, jobject pluginInformation);

jobjectArray queryInstalledPluginsJNI();

void ensureServiceConnectedFromJni(jint connectorInstanceId, std::string& servicePackageName);

int32_t getConnectorInstanceId(aap::PluginClientConnectionList* connections);

} // extern "C"
