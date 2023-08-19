#include "AudioGraph.h"

void aap::BasicAudioGraph::attachNode(AudioGraphNode *sourceNode, int32_t sourceOutputBusIndex,
                                 AudioGraphNode *destinationNode,
                                 int32_t destinationInputBusIndex) {

}

void aap::BasicAudioGraph::detachNode(AudioGraphNode *sourceNode, int32_t sourceOutputBusIndex) {

}

void aap::SimpleLinearAudioGraph::processAudio(void *audioData, int32_t numFrames) {
    file.processAudio(audioData, numFrames);
    plugin.processAudio(audioData, numFrames);
}
