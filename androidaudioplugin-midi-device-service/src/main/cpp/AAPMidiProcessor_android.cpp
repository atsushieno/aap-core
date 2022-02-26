#include "AAPMidiProcessor_android.h"
#include "aap/unstable/logging.h"

namespace aapmidideviceservice {
oboe::DataCallbackResult AAPMidiProcessorOboePAL::onAudioReady(
        oboe::AudioStream *audioStream, void *audioData, int32_t oboeNumFrames) {
    return (oboe::DataCallbackResult) owner->onAudioReady(audioData, oboeNumFrames);
}

int32_t AAPMidiProcessorOboePAL::setupStream() {

    builder.setDirection(oboe::Direction::Output);
    builder.setPerformanceMode(oboe::PerformanceMode::LowLatency);
    builder.setSharingMode(oboe::SharingMode::Exclusive);
    builder.setFormat(oboe::AudioFormat::Float);
    // FIXME: this is incorrect. It should be possible to process stereo outputs from the MIDI synths
    // but need to figure out why it fails to generate valid outputs for the target device.
    builder.setChannelCount(1);// channel_count);
    builder.setChannelConversionAllowed(false);

    builder.setDataCallback(this);

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

