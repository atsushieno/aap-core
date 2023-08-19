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

        virtual AudioDeviceIn* openDefaultInput(uint32_t framesPerCallback) = 0;
        virtual AudioDeviceOut* openDefaultOutput(uint32_t framesPerCallback) = 0;
    };
}


#endif //AAP_CORE_AUDIODEVICEMANAGER_H
