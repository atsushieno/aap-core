#ifndef AAP_CORE_AAPJNIFACADE_H
#define AAP_CORE_AAPJNIFACADE_H

#if ANDROID

#include "aap/core/plugin-information.h"
#include "aap/core/host/plugin-connections.h"
#include "aap/core/host/plugin-host.h"
#include "aap/core/host/plugin-instance.h"
#include "jni.h"

namespace aap {

    int getGuiInstanceSerial();

    class GuiInstance {
    public:

        volatile void* view;
        std::string pluginId;
        int32_t instanceId;
        int32_t externalGuiInstanceId;
        int32_t internalGuiInstanceId;
        std::string lastError{""};
    };

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

        void createGuiViaJni(GuiInstance* guiInstance,
                             std::function<void()> callback);
        void showGuiViaJni(GuiInstance* guiInstance, std::function<void()> callback);
        void hideGuiViaJni(GuiInstance* guiInstance, std::function<void()> callback);
        void destroyGuiViaJni(GuiInstance* guiInstance, std::function<void()> callback);

        void* getWebView(PluginClient* client, RemotePluginInstance* instance);
        void* getNativeView(PluginClient* client, RemotePluginInstance* instance);

        void handleServiceConnectedCallback(std::string servicePackageName);

        jobject getPluginInstanceParameter(jlong nativeHost, jint instanceId, jint index);

        jobject getPluginInstancePort(jlong nativeHost, jint instanceId, jint index);
    };

}

#endif // ANDROID

#endif //AAP_CORE_AAPJNIFACADE_H
