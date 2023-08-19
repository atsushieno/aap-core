#ifndef AAP_CORE_VIRTUALAUDIODEVICEMANAGER_H
#define AAP_CORE_VIRTUALAUDIODEVICEMANAGER_H

#include <cstdint>
#include <memory>

#include "AudioDeviceManager.h"

namespace aap {

    class VirtualAudioDeviceIn : public AudioDeviceIn {
        bool running{false};
    public:
        explicit VirtualAudioDeviceIn() {}
        virtual ~VirtualAudioDeviceIn() {}

        void startCallback() override { running = true; }
        void stopCallback() override { running = false; }
    };

    class VirtualAudioDeviceOut : public AudioDeviceOut {
        bool running{false};
    public:
        explicit VirtualAudioDeviceOut() {}
        virtual ~VirtualAudioDeviceOut() {}

        void startCallback() override { running = true; }
        void stopCallback() override { running = false; }
    };

    class VirtualAudioDeviceManager : public AudioDeviceManager {
        std::shared_ptr<VirtualAudioDeviceIn> input{};
        std::shared_ptr<VirtualAudioDeviceOut> output{};

    public:
        VirtualAudioDeviceManager()
                : input(std::make_shared<VirtualAudioDeviceIn>()),
                  output(std::make_shared<VirtualAudioDeviceOut>()) {
        }

        AudioDeviceIn * openDefaultInput(uint32_t framesPerCallback) override { return input.get(); }
        AudioDeviceOut * openDefaultOutput(uint32_t framesPerCallback) override { return output.get(); }
    };

}


#endif //AAP_CORE_VIRTUALAUDIODEVICEMANAGER_H
