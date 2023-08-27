#include <cassert>
#include "OboeAudioDeviceManager.h"
#include <audio/choc_SampleBuffers.h>
#include <containers/choc_VariableSizeFIFO.h>

namespace aap {
#define AAP_OBOE_IO_TIMEOUT_MILLISECONDS 0

    class OboeAudioDevice : public oboe::StabilizedCallback {

    protected:
        std::shared_ptr<oboe::AudioStream> stream{};
        oboe::AudioStreamBuilder builder{};
        void* callback_context;
        AudioDeviceCallback *aap_callback;
        AudioData aap_buffer;
        void* oboe_buffer;

        oboe::DataCallbackResult onAudioInputReady(oboe::AudioStream *audioStream, void *audioData, int32_t numFrames);
        oboe::DataCallbackResult onAudioOutputReady(oboe::AudioStream *audioStream, void *audioData, int32_t numFrames);

    public:
        explicit OboeAudioDevice(uint32_t framesPerCallback, int32_t numChannels, oboe::Direction direction);

        virtual ~OboeAudioDevice() noexcept;

        void setCallback(AudioDeviceCallback callback, void* callbackContext);

        void startCallback();

        void stopCallback();

        oboe::DataCallbackResult onAudioReady(oboe::AudioStream *audioStream, void *audioData, int32_t numFrames);

        void copyCurrentAAPBufferTo(AudioData *dstAudioData, int32_t bufferPosition, int32_t numFrames);

        void copyAAPBufferForWriting(AudioData *srcAudioData, int32_t currentPosition, int32_t numFrames);
    };

    class OboeAudioDeviceIn :
            public AudioDeviceIn {
        OboeAudioDevice impl;

    public:
        OboeAudioDeviceIn(uint32_t framesPerCallback, int32_t numChannels);
        virtual ~OboeAudioDeviceIn() {}

        // required on Android platform
        bool isPermissionRequired() override { return true; }

        void startCallback() override { impl.startCallback(); }

        void stopCallback() override { impl.stopCallback(); }

        void setAudioCallback(AudioDeviceCallback* callback, void* callbackContext) override {
            impl.setCallback(callback, callbackContext);
        }

        void read(AudioData *audioData, int32_t bufferPosition, int32_t numFrames) override;
    };

    class OboeAudioDeviceOut :
            public AudioDeviceOut {
        OboeAudioDevice impl;

    public:
        OboeAudioDeviceOut(uint32_t framesPerCallback, int32_t numChannels);
        virtual ~OboeAudioDeviceOut() {}

        void startCallback() override { impl.startCallback(); }

        void stopCallback() override { impl.stopCallback(); }

        void setAudioCallback(AudioDeviceCallback* callback, void* callbackContext) override {
            impl.setCallback(callback, callbackContext);
        }

        void write(AudioData *audioDataToWrite, int32_t bufferPosition, int32_t numFrames) override;
    };
}

//--------

aap::AudioDeviceIn *
aap::OboeAudioDeviceManager::openDefaultInput(uint32_t framesPerCallback, int32_t numChannels) {
    assert(input == nullptr);
    input = std::make_shared<OboeAudioDeviceIn>(framesPerCallback, numChannels);
    return input.get();
}

aap::AudioDeviceOut *
aap::OboeAudioDeviceManager::openDefaultOutput(uint32_t framesPerCallback, int32_t numChannels) {
    assert(output == nullptr);
    output = std::make_shared<OboeAudioDeviceOut>(framesPerCallback, numChannels);
    return output.get();
}

//--------

aap::OboeAudioDevice::OboeAudioDevice(uint32_t framesPerCallback, int32_t numChannels, oboe::Direction direction) :
        oboe::StabilizedCallback(this),
        aap_buffer(numChannels, (int32_t) framesPerCallback) {
    builder.setPerformanceMode(oboe::PerformanceMode::LowLatency)
        ->setSharingMode(oboe::SharingMode::Exclusive)
        ->setFormatConversionAllowed(true)
        ->setFormat(oboe::AudioFormat::Float)
        ->setChannelConversionAllowed(true)
        ->setChannelCount(numChannels)
        ->setFramesPerDataCallback(framesPerCallback)
        ->setContentType(oboe::ContentType::Music)
        ->setInputPreset(oboe::InputPreset::Unprocessed)
        ->setDirection(direction)
        ->setCallback(this);

    oboe_buffer = calloc(1, numChannels * framesPerCallback * AAP_MANAGER_AUDIO_QUEUE_NX_FRAMES);
}

aap::OboeAudioDevice::~OboeAudioDevice() noexcept {
    free(oboe_buffer);
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
    if (audioStream->getDirection() == oboe::Direction::Input)
        return onAudioInputReady(audioStream, oboeAudioData, numFrames);
    else
        return onAudioOutputReady(audioStream, oboeAudioData, numFrames);
}

oboe::DataCallbackResult
aap::OboeAudioDevice::onAudioInputReady(oboe::AudioStream *audioStream, void *oboeAudioData,
                                        int32_t numFrames) {
    if (aap_callback != nullptr) {
        auto size = numFrames * audioStream->getChannelCount();
        audioStream->read(oboe_buffer, size, AAP_OBOE_IO_TIMEOUT_MILLISECONDS);
        auto oboeView = choc::buffer::createInterleavedView((float*) oboe_buffer, audioStream->getChannelCount(), numFrames);
        choc::buffer::copyRemappingChannels(aap_buffer.audio.getStart(numFrames), oboeView);

        aap_callback(callback_context, &aap_buffer, numFrames);
    }
    return oboe::DataCallbackResult::Continue;
}

oboe::DataCallbackResult
aap::OboeAudioDevice::onAudioOutputReady(oboe::AudioStream *audioStream, void *oboeAudioData,
                                        int32_t numFrames) {
    if (aap_callback != nullptr) {
        // kick AAP callback, convert AAP result (channel array) to Oboe (interleaved), then write to oboe buffer

        aap_callback(callback_context, &aap_buffer, numFrames);

        auto size = audioStream->getChannelCount() * numFrames;
        auto oboeView = choc::buffer::createInterleavedView((float*) oboe_buffer, audioStream->getChannelCount(), numFrames);
        choc::buffer::copyRemappingChannels(oboeView, aap_buffer.audio.getStart(numFrames));
    }

    return oboe::DataCallbackResult::Continue;
}

void aap::OboeAudioDevice::setCallback(aap::AudioDeviceCallback aapCallback, void *callbackContext) {
    aap_callback = aapCallback;
    callback_context = callbackContext;
}

void aap::OboeAudioDevice::copyCurrentAAPBufferTo(AudioData *dstAudioData, int32_t bufferPosition,
                                                  int32_t numFrames) {
    auto dstView = choc::buffer::createInterleavedView(dstAudioData, stream->getChannelCount(), numFrames);

    // This copies current AAP input buffer (ring buffer) into the argument `dstAudioData`, without "consuming".
    // TODO: implement
}

void aap::OboeAudioDevice::copyAAPBufferForWriting(AudioData *srcAudioData, int32_t currentPosition,
                                                   int32_t numFrames) {
    // This puts `srcAudioData` into current AAP output buffer (ring buffer).
    // TODO: implement
}

//--------

aap::OboeAudioDeviceIn::OboeAudioDeviceIn(uint32_t framesPerCallback, int32_t numChannels) :
        impl(framesPerCallback, numChannels, oboe::Direction::Input) {
}

aap::OboeAudioDeviceOut::OboeAudioDeviceOut(uint32_t framesPerCallback, int32_t numChannels) :
        impl(framesPerCallback, numChannels, oboe::Direction::Output) {

}

void aap::OboeAudioDeviceIn::read(AudioData *dstAudioData, int32_t bufferPosition, int32_t numFrames) {
    impl.copyCurrentAAPBufferTo(dstAudioData, bufferPosition, numFrames);
}

void aap::OboeAudioDeviceOut::write(AudioData *audioDataToWrite, int32_t bufferPosition,
                                    int32_t numFrames) {
    impl.copyAAPBufferForWriting(audioDataToWrite, bufferPosition, numFrames);
}
