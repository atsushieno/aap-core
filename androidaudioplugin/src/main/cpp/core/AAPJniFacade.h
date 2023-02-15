#ifndef AAP_CORE_AAPJNIFACADE_H
#define AAP_CORE_AAPJNIFACADE_H

#include "aap/core/plugin-information.h"
#include "aap/core/host/plugin-connections.h"
#include "jni.h"

namespace aap {

    class AAPJniFacade {

        jclass getAudioPluginMidiSettingsClass();

    public:
        static AAPJniFacade *getInstance();

        void initializeJNIMetadata();

        PluginInformation *
        pluginInformation_fromJava(JNIEnv *env, jobject pluginInformation);

        jobjectArray queryInstalledPluginsJNI();

        void ensureServiceConnectedFromJni(jint connectorInstanceId, std::string servicePackageName,
                                           std::function<void(std::string &)> callback);

        void addScopedClientConnection(int32_t connectorInstanceId, std::string packageName, std::string className, void* connectionData);

        void removeScopedClientConnection(int32_t connectorInstanceId, std::string packageName, std::string className);

        int32_t getConnectorInstanceId(PluginClientConnectionList *connections);

        aap::PluginClientConnectionList *getPluginConnectionListFromJni(jint connectorInstanceId,
                                                                        bool createIfNotExist);

        int32_t getMidiSettingsFromLocalConfig(std::string pluginId);

        void putMidiSettingsToSharedPreference(std::string pluginId, int32_t flags);

        void *createGuiViaJni(std::string pluginId, int32_t instanceId);
        void showGuiViaJni(std::string pluginId, int32_t instanceId, void* view);
        void hideGuiViaJni(std::string pluginId, int32_t instanceId, void* view);
        void destroyGuiViaJni(std::string pluginId, int32_t instanceId, void* view);

        void handleServiceConnectedCallback(std::string servicePackageName);

        jobject getPluginInstanceParameter(jlong nativeHost, jint instanceId, jint index);

        jobject getPluginInstancePort(jlong nativeHost, jint instanceId, jint index);
    };

}
#endif //AAP_CORE_AAPJNIFACADE_H
