#ifndef AAP_CORE_PLUGINPLAYERCONFIGURATION_H
#define AAP_CORE_PLUGINPLAYERCONFIGURATION_H

#include "AudioDevice.h"

#include <cstdint>

namespace aap {

    class PluginPlayerConfiguration {
        AudioDeviceIn* input;
        AudioDeviceOut* output;
        int32_t audio_bus_id;
        int32_t sample_rate;
    public:

        PluginPlayerConfiguration(AudioDeviceIn* input, AudioDeviceOut* output, int32_t audioBusId, int32_t sampleRate) :
            input(input),
            output(output),
            audio_bus_id(audioBusId),
            sample_rate(sampleRate) {
        }
    };

}


#endif //AAP_CORE_PLUGINPLAYERCONFIGURATION_H
