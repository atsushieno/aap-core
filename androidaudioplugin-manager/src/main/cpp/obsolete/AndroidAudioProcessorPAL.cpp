/*
#include <cmath>
#include "AndroidAudioProcessorPAL.h"

oboe::DataCallbackResult
aap::OboeAudioProcessorPAL::onAudioReady(oboe::AudioStream *audioStream, void *audioData,
                                         int32_t numFrames) {
    return (oboe::DataCallbackResult) owner->processAudioIO(audioData, numFrames);
}


bool aap::OboeAudioProcessorPAL::isRunning() {
    return stream_out->getState() == oboe::StreamState::Started;
}

int32_t aap::OboeAudioProcessorPAL::setupStream() {
    builder.setDirection(oboe::Direction::Output)
            ->setPerformanceMode(oboe::PerformanceMode::LowLatency)
            ->setSharingMode(oboe::SharingMode::Exclusive)
            ->setFormat(oboe::AudioFormat::Float)
            // FIXME: this is incorrect. It should be possible to process stereo outputs from the MIDI synths
            // but need to figure out why it fails to generate valid outputs for the target device.
            ->setChannelCount(1) // channel_count);
            ->setChannelConversionAllowed(false)
            ->setFramesPerDataCallback(owner->getBufferSizeInSamples())
            ->setContentType(oboe::ContentType::Music)
            ->setInputPreset(oboe::InputPreset::Unprocessed)
            ->setDataCallback(callback.get());

    return 0;
}

int32_t aap::OboeAudioProcessorPAL::startStreaming() {
    oboe::Result result = builder.openStream(stream_out);
    if (result != oboe::Result::OK) {
        aap::aprintf("Failed to create Oboe stream: %s", oboe::convertToText(result));
        return 1;
    }

    return (int32_t) stream_out->requestStart();
}

int32_t aap::OboeAudioProcessorPAL::stopStreaming() {
    stream_out->stop();
    stream_out->close();
    stream_out.reset();

    return 0;
}

class aap::InterleavedAudioProcessorPAL::Impl {
public:
    AudioProcessor* owner;
    //choc::fifo::SingleReaderSingleWriterFIFO<float> aap_input_ring_buffer{};
    float *interleave_buffer{nullptr};
    struct timespec last_aap_process_time{0, 0};

    Impl(AudioProcessor* owner)
    : owner(owner) {
        auto inChannels = owner->getAudioInChannelCount();
        auto outChannels = owner->getAudioOutChannelCount();
        interleave_buffer = (float *) calloc(
                owner->getBufferSizeInSamples() * (inChannels > outChannels ? inChannels : outChannels),
                sizeof(float));
    }

    void processInterleavedAudioIO(void *audioData, int32_t numFrames) {
        // deinterleave inputs if any

        owner->processAudioIO(audioData, numFrames);

        // interleave outputs
    }
};

aap::InterleavedAudioProcessorPAL::InterleavedAudioProcessorPAL(AudioProcessor* owner) {
    impl = new Impl(owner);
}

aap::InterleavedAudioProcessorPAL::~InterleavedAudioProcessorPAL() {
    delete impl;
}

void
aap::InterleavedAudioProcessorPAL::processInterleavedAudioIO(void *audioData, int32_t numFrames) {
    impl->processInterleavedAudioIO(audioData, 1024);
}
*/
