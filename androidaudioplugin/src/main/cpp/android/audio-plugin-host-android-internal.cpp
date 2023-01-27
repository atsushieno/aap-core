#include "audio-plugin-host-android-internal.h"

namespace aap {

// ----------------------------------

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

std::vector<std::string> AndroidPluginClientSystem::getPluginPaths() {
    std::vector<std::string> ret{};
    for (auto p : queryInstalledPlugins()) {
        assert(p);
        auto packageName = p->getPluginPackageName();
        if (std::find(ret.begin(), ret.end(), packageName) == ret.end())
            ret.emplace_back(packageName);
    }
    return ret;
}

std::vector<PluginInformation*> AndroidPluginClientSystem::getPluginsFromMetadataPaths(std::vector<std::string>& aapMetadataPaths) {
    std::vector<PluginInformation *> results{};
    for (auto p : queryInstalledPlugins())
        if (std::find(aapMetadataPaths.begin(), aapMetadataPaths.end(),
                      p->getPluginPackageName()) != aapMetadataPaths.end())
            results.emplace_back(p);
    return results;
}

void AndroidPluginClientSystem::ensurePluginServiceConnected(aap::PluginClientConnectionList* connections, std::string serviceName, std::function<void(std::string&)> callback) {
    ensureServiceConnectedFromJni(getConnectorInstanceId(connections), serviceName, callback);
}

// -------------------------------------------------------

std::unique_ptr<AndroidPluginClientSystem> android_pal_instance{};

#if ANDROID
PluginClientSystem* PluginClientSystem::getInstance() {
    if (android_pal_instance == nullptr)
        android_pal_instance = std::make_unique<AndroidPluginClientSystem>();
    return android_pal_instance.get();
}
#endif

} // namespace aap
