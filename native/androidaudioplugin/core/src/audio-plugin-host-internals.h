#ifndef AAP_CORE_AUDIO_PLUGIN_HOST_INTERNALS_H
#define AAP_CORE_AUDIO_PLUGIN_HOST_INTERNALS_H

extern "C" {

#if ANDROID
AndroidAudioPluginFactory *(GetAndroidAudioPluginFactoryClientBridge)();
#else
AndroidAudioPluginFactory* (GetDesktopAudioPluginFactoryClientBridge)();
#endif

}

#endif // AAP_CORE_AUDIO_PLUGIN_HOST_INTERNALS_H
