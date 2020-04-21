//
// Created by atsushi on 2020/01/21.
//

#ifndef ANDROIDAUDIOPLUGINFRAMEWORK_ANDROID_AUDIO_PLUGIN_HOST_ANDROID_HPP
#define ANDROIDAUDIOPLUGINFRAMEWORK_ANDROID_AUDIO_PLUGIN_HOST_ANDROID_HPP
#ifdef ANDROID

#include <jni.h>
#include <android/binder_ibinder.h>
#include "aap/android-audio-plugin-host.hpp"

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
    std::vector<std::string> getPluginPaths() override {
		std::vector<std::string> ret{};
		// FIXME: ideally serviceConnections should have a valid list by the time this function is called.
		//  Unfortunately to get it working, bindService() needs to come up with results synchronously,
		//  which is not the case. Therefore we simply accumulate query results and return unique values/
#if true
    	for (auto p : getInstalledPlugins()) {
			if (std::find(ret.begin(), ret.end(), p->getPluginPackageName()) == ret.end())
				ret.emplace_back(p->getPluginPackageName());
    	}
#else
        for (auto c : serviceConnections)
            ret.emplace_back(c.serviceIdentifier);
#endif
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

	// FIXME: move them back to private
	jobject globalApplicationContext{nullptr};
	std::vector<AudioPluginServiceConnection> serviceConnections{};

	JNIEnv* getJNIEnv();

	void initialize(JNIEnv *env, jobject applicationContext);

	void initializeKnownPlugins(jobjectArray jPluginInfos = nullptr);

	AIBinder *getBinderForServiceConnection(std::string packageName, std::string className);
	AIBinder *getBinderForServiceConnectionForPlugin(std::string pluginId);
};

} // namespace aap

#endif
#endif //ANDROIDAUDIOPLUGINFRAMEWORK_ANDROID_AUDIO_PLUGIN_HOST_ANDROID_HPP
