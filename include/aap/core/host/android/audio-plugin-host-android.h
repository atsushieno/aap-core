#ifndef ANDROIDAUDIOPLUGINFRAMEWORK_ANDROID_AUDIO_PLUGIN_HOST_ANDROID_H
#define ANDROIDAUDIOPLUGINFRAMEWORK_ANDROID_AUDIO_PLUGIN_HOST_ANDROID_H
#ifdef ANDROID

#include <jni.h>
#include "../audio-plugin-host.h"

namespace aap {
    aap::PluginClientConnectionList* getPluginConnectionListByConnectorInstanceId(int32_t connectorInstanceId, bool createIfNotExist);

} // namespace aap

#endif // ANDROID
#endif // ANDROIDAUDIOPLUGINFRAMEWORK_ANDROID_AUDIO_PLUGIN_HOST_ANDROID_H
