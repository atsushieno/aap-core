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
    output.getDevice()->startCallback();
}

void aap::SimpleLinearAudioGraph::pauseProcessing() {
    output.getDevice()->stopCallback();
}
