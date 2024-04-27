#include <cassert>
#include "OboeAudioDeviceManager.h"
#include <audio/choc_SampleBuffers.h>
#include <containers/choc_VariableSizeFIFO.h>
#if ANDROID
#include <android/trace.h>
#endif

namespace aap {
#define AAP_OBOE_IO_TIMEOUT_MILLISECONDS 0

    class OboeAudioDevice : public oboe::StabilizedCallback {

    protected:
        std::shared_ptr<oboe::AudioStream> stream{};
        oboe::AudioStreamBuilder builder{};
        void* callback_context{};
        AudioDeviceCallback *aap_callback{};
        AudioBuffer aap_buffer;
        void* oboe_buffer;

        oboe::DataCallbackResult onAudioInputReady(oboe::AudioStream *audioStream, void *audioData, int32_t numFrames);
        oboe::DataCallbackResult onAudioOutputReady(oboe::AudioStream *audioStream, void *audioData, int32_t numFrames);

    public:
        explicit OboeAudioDevice(int32_t sampleRate, int32_t framesPerCallback, int32_t numChannels, oboe::Direction direction);

        ~OboeAudioDevice() noexcept override;

        void setCallback(AudioDeviceCallback callback, void* callbackContext);

        void startCallback();

        void stopCallback();

        oboe::DataCallbackResult onAudioReady(oboe::AudioStream *audioStream, void *audioData, int32_t numFrames) override;

        bool onError(oboe::AudioStream *, oboe::Result) override;

        void copyCurrentAAPBufferTo(AudioBuffer *dstAudioData, int32_t bufferPosition, int32_t numFrames) const;

        void copyAAPBufferForWriting(AudioBuffer *srcAudioData, int32_t currentPosition, int32_t numFrames) const;
    };

    class OboeAudioDeviceIn :
            public AudioDeviceIn {
        OboeAudioDevice impl;

    public:
        OboeAudioDeviceIn(int32_t sampleRate, int32_t framesPerCallback, int32_t numChannels);
        virtual ~OboeAudioDeviceIn() = default;

        // required on Android platform
        bool isPermissionRequired() override { return true; }

        void startCallback() override { impl.startCallback(); }

        void stopCallback() override { impl.stopCallback(); }

        void setAudioCallback(AudioDeviceCallback* callback, void* callbackContext) override {
            impl.setCallback(callback, callbackContext);
        }

        void read(AudioBuffer *audioData, int32_t bufferPosition, int32_t numFrames) override;
    };

    class OboeAudioDeviceOut :
            public AudioDeviceOut {
        OboeAudioDevice impl;

    public:
        OboeAudioDeviceOut(int32_t sampleRate, int32_t framesPerCallback, int32_t numChannels);
        virtual ~OboeAudioDeviceOut() = default;

        void startCallback() override { impl.startCallback(); }

        void stopCallback() override { impl.stopCallback(); }

        void setAudioCallback(AudioDeviceCallback* callback, void* callbackContext) override {
            impl.setCallback(callback, callbackContext);
        }

        void write(AudioBuffer *audioDataToWrite, int32_t bufferPosition, int32_t numFrames) override;
    };
}

//--------

aap::AudioDeviceIn *
aap::OboeAudioDeviceManager::ensureDefaultInputOpened(int32_t sampleRate, int32_t framesPerCallback, int32_t numChannels) {
    if (input == nullptr)
        input = std::make_shared<OboeAudioDeviceIn>(sampleRate, framesPerCallback, numChannels);
    return input.get();
}

aap::AudioDeviceOut *
aap::OboeAudioDeviceManager::ensureDefaultOutputOpened(int32_t sampleRate, int32_t framesPerCallback, int32_t numChannels) {
    if (output == nullptr)
        output = std::make_shared<OboeAudioDeviceOut>(sampleRate, framesPerCallback, numChannels);
    return output.get();
}

//--------

aap::OboeAudioDevice::OboeAudioDevice(int32_t sampleRate, int32_t framesPerCallback, int32_t numChannels, oboe::Direction direction) :
        oboe::StabilizedCallback(this),
        aap_buffer(numChannels, (int32_t) framesPerCallback) {
    builder.setPerformanceMode(oboe::PerformanceMode::LowLatency)
        ->setSharingMode(oboe::SharingMode::Exclusive)
        ->setFormatConversionAllowed(true)
        ->setFormat(oboe::AudioFormat::Float)
        ->setChannelConversionAllowed(true)
        ->setChannelCount(numChannels)
        ->setFramesPerDataCallback(framesPerCallback)
        ->setInputPreset(oboe::InputPreset::Unprocessed)
        ->setDirection(direction)
        ->setSampleRate(sampleRate)
        ->setCallback(this);

    oboe_buffer = calloc(1, numChannels * framesPerCallback * AAP_MANAGER_AUDIO_QUEUE_NX_FRAMES);
}

aap::OboeAudioDevice::~OboeAudioDevice() noexcept {
    aap_callback = nullptr;
    if (stream != nullptr)
        stream->stop(); // we cannot care about closing failure.
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
    if (stream == nullptr)
        return; // already released
    oboe::Result result = stream->stop();
    if (result != oboe::Result::OK)
        throw std::runtime_error(std::string{"Failed to stop Oboe stream: "} + oboe::convertToText(result));
    stream = nullptr;
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
        aap_buffer.audio.clear();
        memset(aap_buffer.midi_in, 0, aap_buffer.midi_capacity);
        memset(aap_buffer.midi_out, 0, aap_buffer.midi_capacity);

        memset(oboeAudioData, 0, numFrames * sizeof(float));

        auto oboeView = choc::buffer::createInterleavedView((float*) oboeAudioData, audioStream->getChannelCount(), numFrames);
        choc::buffer::copy(aap_buffer.audio.getStart(numFrames), oboeView);

        aap_callback(callback_context, &aap_buffer, numFrames);
    }
    return oboe::DataCallbackResult::Continue;
}

const char* local_trace_name = "AAP::OboeAudioDevice::onAudioOutputReady";
oboe::DataCallbackResult
aap::OboeAudioDevice::onAudioOutputReady(oboe::AudioStream *audioStream, void *oboeAudioData,
                                        int32_t numFrames) {
    if (aap_callback != nullptr) {
        // kick AAP callback, convert AAP result (channel array) to Oboe (interleaved), then write to oboe buffer
#if ANDROID
        struct timespec timeSpecBegin{}, timeSpecEnd{};
        if (ATrace_isEnabled()) {
            ATrace_beginSection(local_trace_name);
            clock_gettime(CLOCK_REALTIME, &timeSpecBegin);
        }
#endif

        aap_buffer.audio.clear();
        memset(aap_buffer.midi_in, 0, aap_buffer.midi_capacity);
        memset(aap_buffer.midi_out, 0, aap_buffer.midi_capacity);
        memset(oboeAudioData, 0, numFrames * sizeof(float));

        aap_callback(callback_context, &aap_buffer, numFrames);

        memset(oboeAudioData, 0, numFrames * sizeof(float));

        auto oboeView = choc::buffer::createInterleavedView((float*) oboeAudioData, audioStream->getChannelCount(), numFrames);
        choc::buffer::copy(oboeView, aap_buffer.audio.getStart(numFrames));

#if ANDROID
        if (ATrace_isEnabled()) {
            clock_gettime(CLOCK_REALTIME, &timeSpecEnd);
            ATrace_setCounter(local_trace_name,
                              (timeSpecEnd.tv_sec - timeSpecBegin.tv_sec) * 1000000000 + timeSpecEnd.tv_nsec - timeSpecBegin.tv_nsec);
            ATrace_endSection();
        }
#endif
    }

    return oboe::DataCallbackResult::Continue;
}

void aap::OboeAudioDevice::setCallback(aap::AudioDeviceCallback aapCallback, void *callbackContext) {
    aap_callback = aapCallback;
    callback_context = callbackContext;
}

void aap::OboeAudioDevice::copyCurrentAAPBufferTo(AudioBuffer *dstAudioData, int32_t bufferPosition,
                                                  int32_t numFrames) const {
    // This copies current AAP input buffer (ring buffer) into the argument `dstAudioData`, without "consuming".
    // FIXME: use currentPosition
    // FIXME: use bufferPosition
    choc::buffer::FrameRange range{0, (uint32_t ) numFrames};
    choc::buffer::copy(dstAudioData->audio, aap_buffer.audio.getView().getFrameRange(range));
}

void aap::OboeAudioDevice::copyAAPBufferForWriting(AudioBuffer *srcAudioData, int32_t currentPosition,
                                                   int32_t numFrames) const {
    // This puts `srcAudioData` into current AAP output buffer (ring buffer).
    // FIXME: currentPosition?
    choc::buffer::FrameRange range{0, (uint32_t ) numFrames};
    choc::buffer::copy(aap_buffer.audio.getFrameRange(range), srcAudioData->audio.getView().getFrameRange(range));
}

bool aap::OboeAudioDevice::onError(oboe::AudioStream *audioStream, oboe::Result result) {
    // Should we log errors here?
    return AudioStreamErrorCallback::onError(audioStream, result);
}

//--------

aap::OboeAudioDeviceIn::OboeAudioDeviceIn(int32_t sampleRate, int32_t framesPerCallback, int32_t numChannels) :
        impl(sampleRate, framesPerCallback, numChannels, oboe::Direction::Input) {
}

aap::OboeAudioDeviceOut::OboeAudioDeviceOut(int32_t sampleRate, int32_t framesPerCallback, int32_t numChannels) :
        impl(sampleRate, framesPerCallback, numChannels, oboe::Direction::Output) {

}

void aap::OboeAudioDeviceIn::read(AudioBuffer *dstAudioData, int32_t bufferPosition, int32_t numFrames) {
    impl.copyCurrentAAPBufferTo(dstAudioData, bufferPosition, numFrames);
}

void aap::OboeAudioDeviceOut::write(AudioBuffer *audioDataToWrite, int32_t bufferPosition,
                                    int32_t numFrames) {
    impl.copyAAPBufferForWriting(audioDataToWrite, bufferPosition, numFrames);
}
