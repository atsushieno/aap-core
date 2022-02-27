#include "audio-plugin-host-android.h"

namespace aap {

// ----------------------------------

extern "C" jobjectArray queryInstalledPluginsJNI(); // in AudioPluginHost_native.cpp

extern "C" aap::PluginInformation *
pluginInformation_fromJava(JNIEnv *env, jobject pluginInformation); // in AudioPluginHost_native.cpp

std::vector<PluginInformation*> convertPluginList(jobjectArray jPluginInfos)
{
    std::vector<PluginInformation*> ret;
    assert(jPluginInfos != nullptr);
    JNIEnv *env;
    aap::get_android_jvm()->AttachCurrentThread(&env, nullptr);
    jsize infoSize = env->GetArrayLength(jPluginInfos);
    for (int i = 0; i < infoSize; i++) {
        auto jPluginInfo = (jobject) env->GetObjectArrayElement(jPluginInfos, i);
        ret.emplace_back(pluginInformation_fromJava(env, jPluginInfo));
    }
    return ret;
}

std::vector<PluginInformation*> queryInstalledPlugins() {
    return convertPluginList(queryInstalledPluginsJNI());
}

std::vector<std::string> AndroidPluginHostPAL::getPluginPaths() {
    std::vector<std::string> ret{};
    for (auto p : convertPluginList(queryInstalledPluginsJNI())) {
        if (std::find(ret.begin(), ret.end(), p->getPluginPackageName()) == ret.end())
            ret.emplace_back(p->getPluginPackageName());
    }
    return ret;
}

std::vector<PluginInformation*> AndroidPluginHostPAL::getPluginsFromMetadataPaths(std::vector<std::string>& aapMetadataPaths) {
    std::vector<PluginInformation *> results{};
    for (auto p : queryInstalledPlugins())
        if (std::find(aapMetadataPaths.begin(), aapMetadataPaths.end(),
                      p->getPluginPackageName()) != aapMetadataPaths.end())
            results.emplace_back(p);
    return results;
}

// -------------------------------------------------------

std::unique_ptr<AndroidPluginHostPAL> android_pal_instance{};

PluginHostPAL* getPluginHostPAL()
{
    if (android_pal_instance == nullptr)
        android_pal_instance = std::make_unique<AndroidPluginHostPAL>();
    return android_pal_instance.get();
}

} // namespace aap
