#ifndef AAP_CORE_AUDIO_PLUGIN_HOST_INTERNALS_H
#define AAP_CORE_AUDIO_PLUGIN_HOST_INTERNALS_H

#include "aap/unstable/logging.h"

#if ANDROID
AndroidAudioPluginFactory *GetAndroidAudioPluginFactoryClientBridge(aap::PluginClient *client);
#else
AndroidAudioPluginFactory *GetDesktopAudioPluginFactoryClientBridge(aap::PluginClient *client);
#endif

namespace aap {
    int32_t getMidiSettingsFromLocalConfig(std::string pluginId);
}
#endif // AAP_CORE_AUDIO_PLUGIN_HOST_INTERNALS_H
