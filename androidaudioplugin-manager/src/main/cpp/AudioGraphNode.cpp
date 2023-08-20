#include "AudioGraphNode.h"

void aap::AudioDeviceInputNode::processAudio(void *audioData, int32_t numFrames) {
    assert(false); // TODO, maybe not supported in SimpleLinearAudioGraph
}

//--------

void aap::AudioDeviceOutputNode::processAudio(void *audioData, int32_t numFrames) {
    assert(false); // TODO
}

//--------

bool aap::AudioPluginNode::shouldSkip() {
    return plugin == nullptr;
}

void aap::AudioPluginNode::processAudio(void *audioData, int32_t numFrames) {
    if (!plugin)
        return;
    plugin->process(numFrames, 0); // FIXME: timeout?
}

//--------

bool aap::AudioSourceNode::shouldSkip() {
    return !hasData();
}

void aap::AudioSourceNode::processAudio(void *audioData, int32_t numFrames) {
    if (!hasData())
        return;
    // TODO: copy input audio buffer for numFrames to audioData using read()
}

int32_t aap::AudioDataSourceNode::read(void *dst, int32_t size) {
    return 0;
}

void aap::AudioDataSourceNode::setData(void *audioData, int32_t numFrames, int32_t numChannels) {
    // TODO: atomic locks
    // TODO: convert data to de-interleaved uncompressed binary
    audio_data = audioData;
    frames = numFrames;
    channels = numChannels;
}

void aap::AudioDataSourceNode::setPlaying(bool newPlayingState) {
    playing = newPlayingState;
}

aap::MidiSourceNode::MidiSourceNode(int32_t internalBufferSize) {
    data = (uint8_t*) calloc(1, internalBufferSize);
}

aap::MidiSourceNode::~MidiSourceNode() {
    free(data);
}

void aap::MidiSourceNode::processAudio(void *audioData, int32_t numFrames) {
    // TODO: copy buffered MIDI inputs to audioData
}

void aap::MidiSourceNode::addMidiEvent(uint8_t *data, int32_t length) {
    // TODO: implement
}

aap::MidiDestinationNode::MidiDestinationNode(int32_t internalBufferSize) {
    data = (uint8_t*) calloc(1, internalBufferSize);
}

aap::MidiDestinationNode::~MidiDestinationNode() {
    free(data);
}

void aap::MidiDestinationNode::processAudio(void *audioData, int32_t numFrames) {
    // TODO: copy audioData to buffered MIDI outputs
}
