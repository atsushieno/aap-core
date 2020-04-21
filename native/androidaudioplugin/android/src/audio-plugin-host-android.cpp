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

AIBinder* AndroidPluginHostPAL::getBinderForServiceConnection(std::string packageName, std::string className)
{
    for (int i = 0; i < serviceConnections.size(); i++) {
        auto &s = serviceConnections[i];
        if (s.packageName == packageName && s.className == className)
            return serviceConnections[i].aibinder;
    }
    return nullptr;
}

AIBinder* AndroidPluginHostPAL::getBinderForServiceConnectionForPlugin(std::string pluginId)
{
    auto pl = plugin_list_cache;
    for (int i = 0; pl[i] != nullptr; i++)
        if (pl[i]->getPluginID() == pluginId)
            return getBinderForServiceConnection(pl[i]->getPluginPackageName(), pl[i]->getPluginLocalName());
    return nullptr;
}

extern "C" aap::PluginInformation *
pluginInformation_fromJava(JNIEnv *env, jobject pluginInformation); // in AudioPluginHost_native.cpp

std::vector<PluginInformation*> AndroidPluginHostPAL::convertPluginList(jobjectArray jPluginInfos)
{
    assert(jPluginInfos != nullptr);
    plugin_list_cache.clear();
    auto env = getJNIEnv();
    jsize infoSize = env->GetArrayLength(jPluginInfos);
    for (int i = 0; i < infoSize; i++) {
        auto jPluginInfo = (jobject) env->GetObjectArrayElement(jPluginInfos, i);
        plugin_list_cache.emplace_back(pluginInformation_fromJava(env, jPluginInfo));
    }
    return plugin_list_cache;
}

extern "C" jobjectArray queryInstalledPluginsJNI(); // in AudioPluginHost_native.cpp

void AndroidPluginHostPAL::initializeKnownPlugins(jobjectArray jPluginInfos)
{
    jPluginInfos = jPluginInfos != nullptr ? jPluginInfos : queryInstalledPluginsJNI();
    setPluginListCache(convertPluginList(jPluginInfos));
}

std::vector<aap::PluginInformation*> AndroidPluginHostPAL::queryInstalledPlugins()
{
    return convertPluginList(queryInstalledPluginsJNI());
}

AndroidPluginHostPAL android_pal_instance{};
JavaVM *aap_jvm{nullptr};

// FIXME: it is kind of hack; dynamically loaded and used by JuceAAPWrapper.
extern "C" JavaVM* getJVM() { return aap_jvm; }

PluginHostPAL* getPluginHostPAL()
{
    return &android_pal_instance;
}

JNIEnv* AndroidPluginHostPAL::getJNIEnv()
{
    assert(aap_jvm != nullptr);

    JNIEnv* env;
    aap_jvm->AttachCurrentThread(&env, nullptr);
    return env;
}

void AndroidPluginHostPAL::initialize(JNIEnv *env, jobject applicationContext)
{
    env->GetJavaVM(&aap_jvm);
    globalApplicationContext = applicationContext;
}

} // namespace aap
