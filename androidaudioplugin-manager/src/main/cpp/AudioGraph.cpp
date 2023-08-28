#include "AudioGraph.h"

void aap::SimpleLinearAudioGraph::processAudio(AudioData *audioData, int32_t numFrames) {
    for (auto node : nodes)
        if (!node->shouldSkip())
            node->processAudio(audioData, numFrames);
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

aap::SimpleLinearAudioGraph::SimpleLinearAudioGraph(uint32_t sampleRate, uint32_t framesPerCallback, int32_t channelsInAudioBus) :
        AudioGraph(framesPerCallback, channelsInAudioBus),
        input(this, AudioDeviceManager::getInstance()->openDefaultInput(framesPerCallback, channelsInAudioBus)),
        output(this, AudioDeviceManager::getInstance()->openDefaultOutput(framesPerCallback, channelsInAudioBus)),
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
