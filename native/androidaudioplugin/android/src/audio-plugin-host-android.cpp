//
// Created by atsushi on 2020/01/21.
//

#include <jni.h>
#include <android/log.h>
#include <stdlib.h>
#include <sys/mman.h>
#include "aidl/org/androidaudioplugin/BnAudioPluginInterface.h"
#include "aidl/org/androidaudioplugin/BpAudioPluginInterface.h"
#include "aap/android-audio-plugin-host.hpp"
#include "android-audio-plugin-host-android.hpp"
#include "AudioPluginInterfaceImpl.h"

namespace aap {

AIBinder* AndroidPluginHostPAL::getBinderForServiceConnection(std::string serviceIdentifier)
{
    for (int i = 0; i < serviceConnections.size(); i++)
        if(serviceConnections[i].serviceIdentifier == serviceIdentifier)
            return serviceConnections[i].aibinder;
    return nullptr;
}

AIBinder* AndroidPluginHostPAL::getBinderForServiceConnectionForPlugin(std::string pluginId)
{
    auto pl = getKnownPluginInfos();
    for (int i = 0; pl[i] != nullptr; i++)
        if (pl[i]->getPluginID() == pluginId)
            return getBinderForServiceConnection(pl[i]->getServiceIdentifier());
    return nullptr;
}

const char *interface_descriptor = "org.androidaudioplugin.AudioPluginService";

/*
void* aap_oncreate(void* args)
{
    __android_log_print(ANDROID_LOG_DEBUG, "AAPNativeBridge", "aap_oncreate");
    return NULL;
}

void aap_ondestroy(void* userData)
{
    __android_log_print(ANDROID_LOG_DEBUG, "AAPNativeBridge", "aap_ondestroy");
}

binder_status_t aap_ontransact(AIBinder *binder, transaction_code_t code, const AParcel *in, AParcel *out)
{
    __android_log_print(ANDROID_LOG_DEBUG, "AAPNativeBridge", "aap_ontransact");
    return STATUS_OK;
}
*/

void AndroidPluginHostPAL::cleanupKnownPlugins()
{
    auto localPlugins = getKnownPluginInfos();
    assert(localPlugins != nullptr);
    int n = 0;
    while (localPlugins[n])
        n++;
    for(int i = 0; i < n; i++)
        delete localPlugins[i];
    localPlugins = nullptr;
}

extern "C" aap::PluginInformation *
pluginInformation_fromJava(JNIEnv *env, jobject pluginInformation); // in AudioPluginHost_native.cpp

aap::PluginInformation** AndroidPluginHostPAL::convertPluginList(jobjectArray jPluginInfos)
{
    assert(jPluginInfos != nullptr);
    auto localPlugins = getKnownPluginInfos();
    if(localPlugins)
        cleanupKnownPlugins();
    auto env = getJNIEnv();
    jsize infoSize = env->GetArrayLength(jPluginInfos);
    localPlugins = (aap::PluginInformation **) calloc(sizeof(aap::PluginInformation *), infoSize + 1);
    for (int i = 0; i < infoSize; i++) {
        auto jPluginInfo = (jobject) env->GetObjectArrayElement(jPluginInfos, i);
        localPlugins[i] = pluginInformation_fromJava(env, jPluginInfo);
    }
    localPlugins[infoSize] = nullptr;
    return localPlugins;
}

extern "C" jobjectArray queryInstalledPluginsJNI(); // in AudioPluginHost_native.cpp

void AndroidPluginHostPAL::initializeKnownPlugins(jobjectArray jPluginInfos)
{
    jPluginInfos = jPluginInfos != nullptr ? jPluginInfos : queryInstalledPluginsJNI();
    setKnownPluginInfos(convertPluginList(jPluginInfos));
}

aap::PluginInformation** AndroidPluginHostPAL::queryInstalledPlugins()
{
    return convertPluginList(queryInstalledPluginsJNI());
}

AndroidPluginHostPAL android_pal_instance{};

PluginHostPAL* getPluginHostPAL()
{
    return &android_pal_instance;
}

JNIEnv* AndroidPluginHostPAL::getJNIEnv()
{
    assert(jvm != nullptr);

    JNIEnv* env;
    jvm->AttachCurrentThread(&env, nullptr);
    return env;
}

} // namespace aap
