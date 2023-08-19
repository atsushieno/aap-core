#ifndef AAP_CORE_AUDIODEVICE_H
#define AAP_CORE_AUDIODEVICE_H

#include <cstdint>
#include "LocalDefinitions.h"

#include <oboe/Oboe.h>

namespace aap {

    typedef void(AudioDeviceCallback(void* callbackContext, void* audioData, int32_t numFrames));

    class AAP_PUBLIC_API AudioDeviceIn {
    protected:
        AudioDeviceCallback *callback;

    public:
        AudioDeviceIn() {}

        AAP_PUBLIC_API virtual void startCallback() = 0;
        AAP_PUBLIC_API virtual void stopCallback() = 0;

        void setAudioCallback(AudioDeviceCallback audioDeviceCallback) { callback = audioDeviceCallback; }
    };

    class AAP_PUBLIC_API AudioDeviceOut {
    protected:
        AudioDeviceCallback *callback;

    public:
        AudioDeviceOut() {}

        AAP_PUBLIC_API virtual void startCallback() = 0;
        AAP_PUBLIC_API virtual void stopCallback() = 0;

        void setAudioCallback(AudioDeviceCallback audioDeviceCallback) { callback = audioDeviceCallback; }
    };
}

#endif //AAP_CORE_AUDIODEVICE_H
