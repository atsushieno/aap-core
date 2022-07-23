#ifndef AAP_CORE_AUDIO_PLUGIN_HOST_INTERNALS_H
#define AAP_CORE_AUDIO_PLUGIN_HOST_INTERNALS_H

#include "aap/unstable/logging.h"

#if ANDROID
AndroidAudioPluginFactory *GetAndroidAudioPluginFactoryClientBridge(aap::PluginClient *client);
#else
AndroidAudioPluginFactory* (GetDesktopAudioPluginFactoryClientBridge)();
#endif

namespace aap {

//-----------------------------------

// AndroidAudioPluginBuffer holder that is comfortable in C++ memory model.
class PluginBuffer
{
    std::unique_ptr<AndroidAudioPluginBuffer> buffer{nullptr};

public:
    ~PluginBuffer();

    bool allocateBuffer(size_t numPorts, size_t numFrames, PluginInstance& instance, size_t defaultControlBytesPerBlock);

    AndroidAudioPluginBuffer* getAudioPluginBuffer() { return buffer.get(); }
};

//-----------------------------------

}
#endif // AAP_CORE_AUDIO_PLUGIN_HOST_INTERNALS_H
