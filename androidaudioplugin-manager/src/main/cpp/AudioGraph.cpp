#include "AudioGraph.h"

void aap::BasicAudioGraph::attachNode(AudioGraphNode *sourceNode, int32_t sourceOutputBusIndex,
                                 AudioGraphNode *destinationNode,
                                 int32_t destinationInputBusIndex) {

}

void aap::BasicAudioGraph::detachNode(AudioGraphNode *sourceNode, int32_t sourceOutputBusIndex) {

}

void aap::SimpleLinearAudioGraph::processAudio(void *audioData, int32_t numFrames) {
    if (!audio_data.shouldSkip())
        audio_data.processAudio(audioData, numFrames);
    if (!plugin.shouldSkip())
        plugin.processAudio(audioData, numFrames);
}

void aap::SimpleLinearAudioGraph::setPlugin(aap::RemotePluginInstance *instance) {
    plugin.setPlugin(instance);
}

void aap::SimpleLinearAudioGraph::setAudioData(void *audioData, int32_t numFrames, int32_t numChannels) {
    audio_data.setData(audioData, numChannels, numChannels);
}

void aap::SimpleLinearAudioGraph::addMidiEvent(uint8_t *data, int32_t length) {
    midi_input.addMidiEvent(data, length);
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

aap::SimpleLinearAudioGraph::SimpleLinearAudioGraph(uint32_t framesPerCallback) :
        AudioGraph(framesPerCallback),
        input(this, AudioDeviceManager::getInstance()->openDefaultInput(framesPerCallback)),
        output(this, AudioDeviceManager::getInstance()->openDefaultOutput(framesPerCallback)),
        plugin(this, nullptr),
        audio_data(this, nullptr, 0, 0),
        midi_input(this, AAP_PLUGIN_PLAYER_DEFAULT_MIDI_RING_BUFFER_SIZE),
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
