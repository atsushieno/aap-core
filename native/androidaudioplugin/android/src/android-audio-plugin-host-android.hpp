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
	AudioPluginServiceConnection(std::string serviceIdentifier, jobject jbinder, AIBinder *aibinder)
			: serviceIdentifier(serviceIdentifier), aibinder(aibinder) {}

	std::string serviceIdentifier{};
	jobject jbinder{nullptr};
	AIBinder *aibinder{nullptr};
};

class AndroidPluginHostPAL : public PluginHostPAL
{
	JavaVM *jvm{nullptr};

	aap::PluginInformation** convertPluginList(jobjectArray jPluginInfos);
	aap::PluginInformation** queryInstalledPlugins();

public:
	PluginInformation** getInstalledPlugins() override {
		return queryInstalledPlugins();
	}

	// FIXME: move them back to private
	jobject globalApplicationContext{nullptr};
	std::vector<AudioPluginServiceConnection> serviceConnections{};

	JNIEnv* getJNIEnv();

	void initialize(JNIEnv *env, jobject applicationContext)
	{
		env->GetJavaVM(&jvm);
		globalApplicationContext = applicationContext;
	}

	void initializeKnownPlugins(jobjectArray jPluginInfos = nullptr);
	void cleanupKnownPlugins();

	AIBinder *getBinderForServiceConnection(std::string serviceIdentifier);
	AIBinder *getBinderForServiceConnectionForPlugin(std::string pluginId);
};

} // namespace aap

#endif
#endif //ANDROIDAUDIOPLUGINFRAMEWORK_ANDROID_AUDIO_PLUGIN_HOST_ANDROID_HPP
