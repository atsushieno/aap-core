#include "AAPMidiProcessor_android.h"
#include "aap/unstable/logging.h"

namespace aap::midi {
oboe::DataCallbackResult AAPMidiProcessorOboePAL::onAudioReady(
        oboe::AudioStream *audioStream, void *audioData, int32_t oboeNumFrames) {
    return (oboe::DataCallbackResult) owner->processAudioIO(audioData, oboeNumFrames);
}

int32_t AAPMidiProcessorOboePAL::setupStream() {

    builder.setDirection(oboe::Direction::Output)
        ->setPerformanceMode(oboe::PerformanceMode::LowLatency)
        ->setSharingMode(oboe::SharingMode::Exclusive)
        ->setFormat(oboe::AudioFormat::Float)
        ->setChannelCount(owner->getChannelCount())
        ->setChannelConversionAllowed(false)
        ->setFramesPerDataCallback(owner->getAAPFrameSize())
        ->setContentType(oboe::ContentType::Music)
        ->setInputPreset(oboe::InputPreset::Unprocessed)
        ->setDataCallback(callback.get());

    return 0;
}

int32_t AAPMidiProcessorOboePAL::startStreaming() {
    oboe::Result result = builder.openStream(stream);
    if (result != oboe::Result::OK) {
        aap::aprintf("Failed to create Oboe stream: %s", oboe::convertToText(result));
        return 1;
    }

    return (int32_t) stream->requestStart();
}

int32_t AAPMidiProcessorOboePAL::stopStreaming() {

    // close Oboe stream.
    stream->stop();
    stream->close();
    stream.reset();

    return 0;
}

}

