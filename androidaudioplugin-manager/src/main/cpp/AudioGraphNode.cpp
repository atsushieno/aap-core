#include "AudioGraphNode.h"
#include "AudioGraph.h"

void aap::AudioDeviceInputNode::processAudio(AudioData *audioData, int32_t numFrames) {
    // TODO: copy current audio input data into `audioData`
    getDevice()->readAAPNodeBuffer(audioData, consumer_position, numFrames);
    // TODO: adjust ring buffer offset, not just simple adder.
    consumer_position += numFrames;
}

void aap::AudioDeviceInputNode::start() {
    if (!shouldSkip())
        getDevice()->startCallback();
}

void aap::AudioDeviceInputNode::pause() {
    if (!shouldSkip())
        getDevice()->stopCallback();
}

void aap::AudioDeviceInputNode::setPermissionGranted() {
    permission_granted = true;
}

bool aap::AudioDeviceInputNode::shouldSkip() {
    return getDevice()->isPermissionRequired() && !permission_granted;
}

//--------

void aap::AudioDeviceOutputNode::processAudio(AudioData *audioData, int32_t numFrames) {
    // TODO: copy `audioData` into current audio output buffer
    getDevice()->writeToPlatformBuffer(audioData, consumer_position, numFrames);
    // TODO: adjust ring buffer offset, not just simple adder.
    consumer_position += numFrames;
}

void aap::AudioDeviceOutputNode::start() {
    getDevice()->startCallback();
}

void aap::AudioDeviceOutputNode::pause() {
    getDevice()->stopCallback();
}

//--------

bool aap::AudioPluginNode::shouldSkip() {
    return plugin == nullptr;
}

void aap::AudioPluginNode::processAudio(AudioData *audioData, int32_t numFrames) {
    if (!plugin)
        return;
    plugin->process(numFrames, 0); // FIXME: timeout?
}

void aap::AudioPluginNode::start() {
    plugin->prepare(graph->getFramesPerCallback());
    plugin->activate();
}

void aap::AudioPluginNode::pause() {
    plugin->deactivate();
}

//--------

bool aap::AudioSourceNode::shouldSkip() {
    return !active || !hasData();
}

void aap::AudioSourceNode::processAudio(AudioData *audioData, int32_t numFrames) {
    // TODO: copy input audio buffer for numFrames to audioData using read()
}

void aap::AudioSourceNode::start() {
    active = true;
}

void aap::AudioSourceNode::pause() {
    active = false;
}

void aap::AudioSourceNode::setPlaying(bool newPlayingState) {
    playing = newPlayingState;
}

//--------

int32_t aap::AudioDataSourceNode::read(AudioData *dst, int32_t numFrames) {
    // TODO: correct implementation
    memcpy(dst, audio_data, graph->getAAPChannelCount(graph->getChannelsInAudioBus()) * numFrames);
    return 0;
}

void aap::AudioDataSourceNode::setData(AudioData *audioData, int32_t numFrames, int32_t numChannels) {
    // FIXME: atomic locks
    // TODO: convert data to de-interleaved uncompressed binary
    audio_data = audioData;
    frames = numFrames;
    channels = numChannels;
}

//--------

aap::MidiSourceNode::MidiSourceNode(AudioGraph* ownerGraph, int32_t internalBufferSize) :
        AudioGraphNode(ownerGraph) {
    buffer = (uint8_t*) calloc(1, internalBufferSize);
    capacity = internalBufferSize;
}

aap::MidiSourceNode::~MidiSourceNode() {
    free(buffer);
}

void aap::MidiSourceNode::processAudio(AudioData *audioData, int32_t numFrames) {
    // TODO: copy buffered MIDI inputs to audioData
}

void aap::MidiSourceNode::addMidiEvent(uint8_t *data, int32_t length) {
    if (producer_offset + length < capacity)
        memcpy(buffer + producer_offset, data, length);
    // FIXME: log error for insufficient event buffer
}

void aap::MidiSourceNode::start() {
}

void aap::MidiSourceNode::pause() {

}

//--------

aap::MidiDestinationNode::MidiDestinationNode(AudioGraph* ownerGraph, int32_t internalBufferSize) :
        AudioGraphNode(ownerGraph) {
    buffer = (uint8_t*) calloc(1, internalBufferSize);
}

aap::MidiDestinationNode::~MidiDestinationNode() {
    free(buffer);
}

void aap::MidiDestinationNode::processAudio(AudioData *audioData, int32_t numFrames) {
    // TODO: copy audioData to buffered MIDI outputs
}

void aap::MidiDestinationNode::start() {

}

void aap::MidiDestinationNode::pause() {

}
