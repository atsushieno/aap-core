#include <cassert>
#include "OboeAudioDeviceManager.h"

namespace aap {

    class OboeAudioDevice : public oboe::StabilizedCallback {
    protected:
        std::shared_ptr<oboe::AudioStream> stream{};
        oboe::AudioStreamBuilder builder{};
        void* callback_context{nullptr};
        AudioDeviceCallback *aap_callback;
        oboe::AudioStream *current_audio_stream;
        void *current_oboe_data;
        void *current_aap_data;

    public:
        explicit OboeAudioDevice(uint32_t framesPerCallback, oboe::Direction direction);

        void setCallback(AudioDeviceCallback callback, void* callbackContext);

        void startCallback();

        void stopCallback();

        oboe::DataCallbackResult
        onAudioReady(oboe::AudioStream *audioStream, void *audioData, int32_t numFrames);

        void copyCurrentAAPBufferTo(void *dstAudioData, int32_t bufferPosition, int32_t numFrames);

        void copyAAPBufferForWriting(void *srcAudioData, int32_t currentPosition, int32_t numFrames);
    };

    class OboeAudioDeviceIn :
            public AudioDeviceIn {
        OboeAudioDevice impl;

    public:
        OboeAudioDeviceIn(uint32_t framesPerCallback);
        virtual ~OboeAudioDeviceIn() {}

        // required on Android platform
        bool isPermissionRequired() override { return true; }

        void startCallback() override { impl.startCallback(); }

        void stopCallback() override { impl.stopCallback(); }

        void setAudioCallback(AudioDeviceCallback* callback, void* callbackContext) override {
            impl.setCallback(callback, callbackContext);
        }

        void readAAPNodeBuffer(void *audioData, int32_t bufferPosition, int32_t numFrames) override;
    };

    class OboeAudioDeviceOut :
            public AudioDeviceOut {
        OboeAudioDevice impl;

    public:
        OboeAudioDeviceOut(uint32_t framesPerCallback);
        virtual ~OboeAudioDeviceOut() {}

        void startCallback() override { impl.startCallback(); }

        void stopCallback() override { impl.stopCallback(); }

        void setAudioCallback(AudioDeviceCallback* callback, void* callbackContext) override {
            impl.setCallback(callback, callbackContext);
        }

        void writeToPlatformBuffer(void *audioData, int32_t bufferPosition, int32_t numFrames) override;
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

aap::OboeAudioDevice::OboeAudioDevice(uint32_t framesPerCallback, oboe::Direction direction) :
        oboe::StabilizedCallback(this) {
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
            ->setDirection(direction)
            ->setCallback(this);
}

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

oboe::DataCallbackResult
aap::OboeAudioDevice::onAudioReady(oboe::AudioStream *audioStream, void *oboeAudioData,
                                      int32_t numFrames) {
    if (aap_callback != nullptr) {
        current_audio_stream = audioStream;
        current_oboe_data = oboeAudioData;

        // FIXME: convert Oboe AudioStream to our buffer structure
        current_aap_data = oboeAudioData;

        aap_callback(callback_context, current_aap_data, numFrames);
    }
    return oboe::DataCallbackResult::Continue;
}

void aap::OboeAudioDevice::setCallback(aap::AudioDeviceCallback aapCallback, void *callbackContext) {
    aap_callback = aapCallback;
    callback_context = callbackContext;
}

void aap::OboeAudioDevice::copyCurrentAAPBufferTo(void *dstAudioData, int32_t bufferPosition,
                                                  int32_t numFrames) {
    // This copies current AAP input buffer (ring buffer) into the argument `dstAudioData`, without "consuming".
    // TODO: implement
}

void aap::OboeAudioDevice::copyAAPBufferForWriting(void *srcAudioData, int32_t currentPosition,
                                                   int32_t numFrames) {
    // This puts `srcAudioData` into current AAP output buffer (ring buffer).
    // TODO: implement
}

//--------

aap::OboeAudioDeviceIn::OboeAudioDeviceIn(uint32_t framesPerCallback) :
        impl(framesPerCallback, oboe::Direction::Input) {
}

aap::OboeAudioDeviceOut::OboeAudioDeviceOut(uint32_t framesPerCallback) :
        impl(framesPerCallback, oboe::Direction::Output) {

}

void aap::OboeAudioDeviceIn::readAAPNodeBuffer(void *audioData, int32_t bufferPosition, int32_t numFrames) {
    impl.copyCurrentAAPBufferTo(audioData, bufferPosition, numFrames);
}

void aap::OboeAudioDeviceOut::writeToPlatformBuffer(void *audioData, int32_t bufferPosition,
                                                int32_t numFrames) {
    impl.copyAAPBufferForWriting(audioData, bufferPosition, numFrames);
}
