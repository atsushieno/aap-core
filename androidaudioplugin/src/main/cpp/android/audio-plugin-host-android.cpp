#include "audio-plugin-host-android.h"

namespace aap {

// ----------------------------------

std::vector<std::string> AndroidPluginHostPAL::getPluginPaths() {
    // Due to the way how Android service query works (asynchronous),
    // it has to wait until AudioPluginHost service query completes.
    for (int i = 0; i < SERVICE_QUERY_TIMEOUT_IN_SECONDS; i++)
        if (plugin_list_cache.empty())
            sleep(1);
    std::vector<std::string> ret{};
    for (auto p : plugin_list_cache) {
        if (std::find(ret.begin(), ret.end(), p->getPluginPackageName()) == ret.end())
            ret.emplace_back(p->getPluginPackageName());
    }
    return ret;
}

std::vector<PluginInformation*> AndroidPluginHostPAL::getPluginsFromMetadataPaths(std::vector<std::string>& aapMetadataPaths) {
    std::vector<PluginInformation *> results{};
    for (auto p : plugin_list_cache)
        if (std::find(aapMetadataPaths.begin(), aapMetadataPaths.end(),
                      p->getPluginPackageName()) != aapMetadataPaths.end())
            results.emplace_back(p);
    return results;
}

AIBinder* AndroidPluginHostPAL::getBinderForServiceConnection(std::string packageName, std::string className)
{
    for (int i = 0; i < serviceConnections.size(); i++) {
        auto s = serviceConnections[i].get();
        if (s->getPackageName() == packageName && s->getClassName() == className)
            return (AIBinder*) serviceConnections[i]->getConnectionData();
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
    JNIEnv *env;
    aap::get_android_jvm()->AttachCurrentThread(&env, nullptr);
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

std::unique_ptr<AndroidPluginHostPAL> android_pal_instance{};

PluginHostPAL* getPluginHostPAL()
{
    if (android_pal_instance == nullptr)
        android_pal_instance = std::make_unique<AndroidPluginHostPAL>();
    return android_pal_instance.get();
}

} // namespace aap
