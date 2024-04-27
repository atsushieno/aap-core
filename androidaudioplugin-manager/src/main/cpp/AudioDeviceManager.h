#ifndef AAP_CORE_AUDIODEVICEMANAGER_H
#define AAP_CORE_AUDIODEVICEMANAGER_H

#include <stdint.h>
#include <memory>

#include "AudioDevice.h"

namespace aap {
    class AudioDeviceManager {
    public:
        // This needs to be implemented for each platform
        static AudioDeviceManager* getInstance();

        virtual AudioDeviceIn* ensureDefaultInputOpened(int32_t sampleRate, int32_t framesPerCallback, int32_t numChannels) = 0;
        virtual AudioDeviceOut* ensureDefaultOutputOpened(int32_t sampleRate, int32_t framesPerCallback, int32_t numChannels) = 0;
    };
}


#endif //AAP_CORE_AUDIODEVICEMANAGER_H
