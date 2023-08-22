#ifndef AAP_CORE_AUDIODEVICE_H
#define AAP_CORE_AUDIODEVICE_H

#include <cstdint>
#include "LocalDefinitions.h"

#include <oboe/Oboe.h>

namespace aap {

    typedef void(AudioDeviceCallback(void* callbackContext, void* audioData, int32_t numFrames));

    class AAP_PUBLIC_API AudioDeviceIn {

    public:
        AudioDeviceIn() {}

        AAP_PUBLIC_API virtual void startCallback() = 0;
        AAP_PUBLIC_API virtual void stopCallback() = 0;

        virtual void setAudioCallback(AudioDeviceCallback audioDeviceCallback, void* callbackContext) = 0;

        virtual void readAAPNodeBuffer(void *audioData, int32_t bufferPosition, int32_t numFrames) = 0;

        virtual bool isPermissionRequired() { return false; }
    };

    class AAP_PUBLIC_API AudioDeviceOut {
    public:
        AudioDeviceOut() {}

        AAP_PUBLIC_API virtual void startCallback() = 0;
        AAP_PUBLIC_API virtual void stopCallback() = 0;

        virtual void setAudioCallback(AudioDeviceCallback audioDeviceCallback, void* callbackContext) = 0;

        virtual void writeToPlatformBuffer(void *audioData, int32_t bufferPosition, int32_t numFrames) = 0;
    };
}

#endif //AAP_CORE_AUDIODEVICE_H
