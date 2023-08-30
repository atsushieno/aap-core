#include "AudioGraph.h"
#if ANDROID
#include <android/trace.h>
#endif

void aap::SimpleLinearAudioGraph::processAudio(AudioBuffer *audioData, int32_t numFrames) {
    struct timespec timeSpecBegin{}, timeSpecEnd{};
#if ANDROID
    if (ATrace_isEnabled()) {
        ATrace_beginSection("AAP::SimpleLinearAudioGraph_processAudio");
        clock_gettime(CLOCK_REALTIME, &timeSpecBegin);
    }
#endif

    for (auto node : nodes)
        if (!node->shouldSkip())
            node->processAudio(audioData, numFrames);

#if ANDROID
    if (ATrace_isEnabled()) {
        clock_gettime(CLOCK_REALTIME, &timeSpecEnd);
        ATrace_setCounter("AAP::SimpleLinearAudioGraph_processAudio",
                          (timeSpecEnd.tv_sec - timeSpecBegin.tv_sec) * 1000000000 + timeSpecEnd.tv_nsec - timeSpecBegin.tv_nsec);
        ATrace_endSection();
    }
#endif

}

void aap::SimpleLinearAudioGraph::setPlugin(aap::RemotePluginInstance *instance) {
    plugin.setPlugin(instance);
}

void
aap::SimpleLinearAudioGraph::setAudioSource(uint8_t *data, int dataLength, const char *filename) {
    audio_data.setAudioSource(data, dataLength, filename);
}

void aap::SimpleLinearAudioGraph::addMidiEvent(uint8_t *data, int32_t length, int64_t timestampInNanoseconds) {
    midi_input.addMidiEvent(data, length, timestampInNanoseconds);
}

void aap::SimpleLinearAudioGraph::playAudioData() {
    audio_data.setPlaying(true);
}

void aap::SimpleLinearAudioGraph::startProcessing() {
    for (auto node : nodes)
        node->start();
}

void aap::SimpleLinearAudioGraph::pauseProcessing() {
    for (auto node : nodes)
        node->pause();
}

aap::SimpleLinearAudioGraph::SimpleLinearAudioGraph(int32_t sampleRate, uint32_t framesPerCallback, int32_t channelsInAudioBus) :
        AudioGraph(sampleRate, framesPerCallback, channelsInAudioBus),
        input(this, AudioDeviceManager::getInstance()->openDefaultInput(sampleRate, framesPerCallback, channelsInAudioBus)),
        output(this, AudioDeviceManager::getInstance()->openDefaultOutput(sampleRate, framesPerCallback, channelsInAudioBus)),
        plugin(this, nullptr),
        audio_data(this),
        midi_input(this, nullptr, sampleRate, framesPerCallback, AAP_PLUGIN_PLAYER_DEFAULT_MIDI_RING_BUFFER_SIZE),
        midi_output(this, AAP_PLUGIN_PLAYER_DEFAULT_MIDI_RING_BUFFER_SIZE) {
    nodes.emplace_back(&input);
    nodes.emplace_back(&audio_data);
    nodes.emplace_back(&midi_input);
    nodes.emplace_back(&plugin);
    nodes.emplace_back(&midi_output);
    nodes.emplace_back(&output);

    output.getDevice()->setAudioCallback(audio_callback, this);
}

void aap::SimpleLinearAudioGraph::enableAudioRecorder() {
    input.setPermissionGranted();
}
