#ifndef ANDROIDAUDIOPLUGINFRAMEWORK_ANDROID_AUDIO_PLUGIN_HOST_ANDROID_H
#define ANDROIDAUDIOPLUGINFRAMEWORK_ANDROID_AUDIO_PLUGIN_HOST_ANDROID_H
#ifdef ANDROID

#include <jni.h>
#include <android/binder_ibinder.h>
#include <android/sharedmem.h>
#include "../audio-plugin-host.h"

#define SERVICE_QUERY_TIMEOUT_IN_SECONDS 10

namespace aap {

class AudioPluginServiceConnection {
public:
	AudioPluginServiceConnection(std::string packageName, std::string className, AIBinder *aibinder)
			: packageName(packageName), className(className), aibinder(aibinder) {}

	std::string packageName;
	std::string className;
	AIBinder *aibinder;
};

class AndroidPluginHostPAL : public PluginHostPAL
{
	std::vector<PluginInformation*> convertPluginList(jobjectArray jPluginInfos);
	std::vector<PluginInformation*> queryInstalledPlugins();

public:
	virtual inline ~AndroidPluginHostPAL() {}

	inline int32_t createSharedMemory(size_t size) override { return ASharedMemory_create(nullptr, size); }

	std::vector<std::string> getPluginPaths() override;

    inline void getAAPMetadataPaths(std::string path, std::vector<std::string>& results) override {
        // On Android there is no access to directories for each service. Only service identifier is passed.
        // Therefore, we simply add "path" which actually is a service identifier, to the results.
        results.emplace_back(path);
    }

    std::vector<PluginInformation*> getPluginsFromMetadataPaths(std::vector<std::string>& aapMetadataPaths) override;

	std::vector<std::unique_ptr<AudioPluginServiceConnection>> serviceConnections{};

	JNIEnv* getJNIEnv();

	void initialize(JNIEnv *env, jobject applicationContext);
	void terminate(JNIEnv *env);

	void initializeKnownPlugins(jobjectArray jPluginInfos = nullptr);

	AIBinder *getBinderForServiceConnection(std::string packageName, std::string className);
	AIBinder *getBinderForServiceConnectionForPlugin(std::string pluginId);
};

} // namespace aap

#endif // ANDROID
#endif // ANDROIDAUDIOPLUGINFRAMEWORK_ANDROID_AUDIO_PLUGIN_HOST_ANDROID_H
