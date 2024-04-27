#ifndef AAP_CORE_VIRTUALAUDIODEVICEMANAGER_H
#define AAP_CORE_VIRTUALAUDIODEVICEMANAGER_H

#include <cstdint>
#include <memory>

#include "AudioDeviceManager.h"

namespace aap {

    class VirtualAudioDeviceIn : public AudioDeviceIn {
        bool running{false};
    public:
        explicit VirtualAudioDeviceIn() = default;
        virtual ~VirtualAudioDeviceIn() = default;

        void startCallback() override { running = true; }
        void stopCallback() override { running = false; }
        void setAudioCallback(AudioDeviceCallback audioDeviceCallback, void* callbackContext) override {
            // should we implement something here?
        }
        void read(AudioBuffer *dstAudioData, int32_t bufferPosition, int32_t numFrames) override {
            // should we implement something here?
        }
    };

    class VirtualAudioDeviceOut : public AudioDeviceOut {
        bool running{false};
    public:
        explicit VirtualAudioDeviceOut() = default;
        virtual ~VirtualAudioDeviceOut() = default;

        void startCallback() override { running = true; }
        void stopCallback() override { running = false; }
        void setAudioCallback(AudioDeviceCallback audioDeviceCallback, void* callbackContext) override {
            // should we implement something here?
        }
        void write(AudioBuffer *audioDataToWrite, int32_t bufferPosition, int32_t numFrames) override {
            // should we implement something here?
        }
    };

    class VirtualAudioDeviceManager : public AudioDeviceManager {
        std::shared_ptr<VirtualAudioDeviceIn> input{};
        std::shared_ptr<VirtualAudioDeviceOut> output{};

    public:
        VirtualAudioDeviceManager()
                : input(std::make_shared<VirtualAudioDeviceIn>()),
                  output(std::make_shared<VirtualAudioDeviceOut>()) {
        }

        AudioDeviceIn * openDefaultInput(int32_t sampleRate, int32_t framesPerCallback, int32_t numChannels) override { return input.get(); }
        AudioDeviceOut * openDefaultOutput(int32_t sampleRate, int32_t framesPerCallback, int32_t numChannels) override { return output.get(); }
    };

}


#endif //AAP_CORE_VIRTUALAUDIODEVICEMANAGER_H
