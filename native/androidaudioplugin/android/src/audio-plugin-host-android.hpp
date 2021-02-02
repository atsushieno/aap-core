//
// Created by atsushi on 2020/01/21.
//

#ifndef ANDROIDAUDIOPLUGINFRAMEWORK_ANDROID_AUDIO_PLUGIN_HOST_ANDROID_HPP
#define ANDROIDAUDIOPLUGINFRAMEWORK_ANDROID_AUDIO_PLUGIN_HOST_ANDROID_HPP
#ifdef ANDROID

#include <jni.h>
#include <android/binder_ibinder.h>
#include "aap/android-audio-plugin-host.hpp"

#define SERVICE_QUERY_TIMEOUT_IN_SECONDS 10

namespace aap {

class AudioPluginServiceConnection {
public:
	AudioPluginServiceConnection(std::string packageName, std::string className, jobject jbinder, AIBinder *aibinder)
			: packageName(packageName), className(className), aibinder(aibinder) {}

	std::string packageName{};
	std::string className{};
	jobject jbinder{nullptr};
	AIBinder *aibinder{nullptr};
};

class AndroidPluginHostPAL : public PluginHostPAL
{
	std::vector<PluginInformation*> convertPluginList(jobjectArray jPluginInfos);
	std::vector<PluginInformation*> queryInstalledPlugins();

public:
	std::string getRemotePluginEntrypoint() override { return "GetAndroidAudioPluginFactoryClientBridge"; }
	std::vector<std::string> getPluginPaths() override {
		// Due to the way how Android service query works (asynchronous),
		// it has to wait until AudioPluginHost service query completes.
		for (int i = 0; i < SERVICE_QUERY_TIMEOUT_IN_SECONDS; i++)
			if (plugin_list_cache.size() == 0)
				sleep(1);
		std::vector<std::string> ret{};
    	for (auto p : plugin_list_cache) {
			if (std::find(ret.begin(), ret.end(), p->getPluginPackageName()) == ret.end())
				ret.emplace_back(p->getPluginPackageName());
    	}
		return ret;
    }

    void getAAPMetadataPaths(std::string path, std::vector<std::string>& results) override {
        // On Android there is no access to directories for each service. Only service identifier is passed.
        // Therefore, we simply add "path" which actually is a service identifier, to the results.
        results.emplace_back(path);
    }

    std::vector<PluginInformation*> getPluginsFromMetadataPaths(std::vector<std::string>& aapMetadataPaths) override {
        std::vector<PluginInformation*> results{};
        for (auto p : plugin_list_cache)
            if (std::find(aapMetadataPaths.begin(), aapMetadataPaths.end(), p->getPluginPackageName()) != aapMetadataPaths.end())
                results.emplace_back(p);
        return results;
    }

	std::vector<AudioPluginServiceConnection> serviceConnections{};

	JNIEnv* getJNIEnv();

	void initialize(JNIEnv *env, jobject applicationContext);
	void terminate(JNIEnv *env);

	void initializeKnownPlugins(jobjectArray jPluginInfos = nullptr);

	AIBinder *getBinderForServiceConnection(std::string packageName, std::string className);
	AIBinder *getBinderForServiceConnectionForPlugin(std::string pluginId);
};

} // namespace aap

#endif
#endif //ANDROIDAUDIOPLUGINFRAMEWORK_ANDROID_AUDIO_PLUGIN_HOST_ANDROID_HPP
