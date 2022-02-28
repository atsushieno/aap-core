#ifndef ANDROIDAUDIOPLUGINFRAMEWORK_ANDROID_AUDIO_PLUGIN_HOST_ANDROID_H
#define ANDROIDAUDIOPLUGINFRAMEWORK_ANDROID_AUDIO_PLUGIN_HOST_ANDROID_H
#ifdef ANDROID

#include <jni.h>
#include <android/binder_ibinder.h>
#include <android/sharedmem.h>
#include "../audio-plugin-host.h"

extern "C" aap::PluginClientConnectionList* getPluginConnectionListFromJni(jint connectorInstanceId, bool createIfNotExist);

namespace aap {

} // namespace aap

#endif // ANDROID
#endif // ANDROIDAUDIOPLUGINFRAMEWORK_ANDROID_AUDIO_PLUGIN_HOST_ANDROID_H
