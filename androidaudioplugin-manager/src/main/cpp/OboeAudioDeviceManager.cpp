#include <cassert>
#include "OboeAudioDeviceManager.h"

namespace aap {

    class OboeAudioDevice {
    protected:
        std::shared_ptr<oboe::AudioStream> stream{};
        oboe::AudioStreamBuilder builder{};
        std::unique_ptr<oboe::StabilizedCallback> callback{};

    public:
        explicit OboeAudioDevice(uint32_t framesPerCallback);

        void startCallback();

        void stopCallback();
        oboe::DataCallbackResult
        onAudioReady(oboe::AudioStream *audioStream, void *audioData, int32_t numFrames);
    };

    class OboeAudioDeviceIn :
            public AudioDeviceIn,
            public oboe::AudioStreamDataCallback,
            OboeAudioDevice {

    public:
        OboeAudioDeviceIn(uint32_t framesPerCallback);
        virtual ~OboeAudioDeviceIn() {}

        void startCallback() override { ((OboeAudioDevice*) this)->startCallback(); }

        void stopCallback() override { ((OboeAudioDevice*) this)->stopCallback(); }

        oboe::DataCallbackResult
        onAudioReady(oboe::AudioStream *audioStream, void *audioData, int32_t numFrames) override {
            return ((OboeAudioDevice*) this)->onAudioReady(audioStream, audioData, numFrames);
        }
    };

    class OboeAudioDeviceOut :
            public AudioDeviceOut,
            public oboe::AudioStreamDataCallback,
            OboeAudioDevice {
    public:
        OboeAudioDeviceOut(uint32_t framesPerCallback);
        virtual ~OboeAudioDeviceOut() {}

        void startCallback() override { ((OboeAudioDevice*) this)->startCallback(); }

        void stopCallback() override { ((OboeAudioDevice*) this)->stopCallback(); }

        oboe::DataCallbackResult
        onAudioReady(oboe::AudioStream *audioStream, void *audioData, int32_t numFrames) override {
            return ((OboeAudioDevice*) this)->onAudioReady(audioStream, audioData, numFrames);
        }
    };
}

//--------

aap::AudioDeviceIn *
aap::OboeAudioDeviceManager::openDefaultInput(uint32_t framesPerCallback) {
    assert(input == nullptr);
    input = std::make_shared<OboeAudioDeviceIn>(framesPerCallback);
    return input.get();
}

aap::AudioDeviceOut *
aap::OboeAudioDeviceManager::openDefaultOutput(uint32_t framesPerCallback) {
    assert(output == nullptr);
    output = std::make_shared<OboeAudioDeviceOut>(framesPerCallback);
    return output.get();
}

//--------

void aap::OboeAudioDevice::startCallback() {
    oboe::Result result = builder.openStream(stream);
    if (result != oboe::Result::OK)
        throw std::runtime_error(std::string{"Failed to create Oboe stream: "} + oboe::convertToText(result));

    result = stream->requestStart();
    if (result != oboe::Result::OK)
        throw std::runtime_error(std::string{"Failed to start Oboe stream: "} + oboe::convertToText(result));
}

void aap::OboeAudioDevice::stopCallback() {
    assert(stream != nullptr);
    oboe::Result result = stream->stop();
    if (result != oboe::Result::OK)
        throw std::runtime_error(std::string{"Failed to stop Oboe stream: "} + oboe::convertToText(result));
}

//--------

aap::OboeAudioDevice::OboeAudioDevice(uint32_t framesPerCallback) {
    builder.setPerformanceMode(oboe::PerformanceMode::LowLatency)
            ->setSharingMode(oboe::SharingMode::Exclusive)
            ->setFormat(oboe::AudioFormat::Float)
                    // FIXME: this is incorrect. It should be possible to process stereo outputs from the MIDI synths
                    // but need to figure out why it fails to generate valid outputs for the target device.
            ->setChannelCount(1) // channel_count);
            ->setChannelConversionAllowed(false)
            ->setFramesPerDataCallback(framesPerCallback)
            ->setContentType(oboe::ContentType::Music)
            ->setInputPreset(oboe::InputPreset::Unprocessed)
            ->setDataCallback(callback.get());
}

aap::OboeAudioDeviceIn::OboeAudioDeviceIn(uint32_t framesPerCallback) :
        OboeAudioDevice(framesPerCallback) {
    builder.setDirection(oboe::Direction::Input);
}

aap::OboeAudioDeviceOut::OboeAudioDeviceOut(uint32_t framesPerCallback) :
        OboeAudioDevice(framesPerCallback) {
    builder.setDirection(oboe::Direction::Output);
}

//--------

oboe::DataCallbackResult
aap::OboeAudioDevice::onAudioReady(oboe::AudioStream *audioStream, void *audioData,
                                      int32_t numFrames) {

    return oboe::DataCallbackResult::Continue;
}
