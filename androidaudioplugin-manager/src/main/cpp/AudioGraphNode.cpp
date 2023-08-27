#include "AudioGraphNode.h"
#include "AudioGraph.h"

void aap::AudioDeviceInputNode::processAudio(AudioData *audioData, int32_t numFrames) {
    // TODO: copy current audio input data into `audioData`
    getDevice()->read(audioData, consumer_position, numFrames);
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
    getDevice()->write(audioData, consumer_position, numFrames);
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

    // Copy input audioData into each plugin's buffer (it is inevitable; each plugin has
    // shared memory between the service and this host, which are not sharable with other plugins
    // in the chain. So, it's optimal enough.)

    auto aapBuffer = plugin->getAudioPluginBuffer();

    int32_t currentChannelInAudioData = 0;
    for (int32_t i = 0, n = aapBuffer->num_ports(*aapBuffer); i < n; i++) {
        switch (plugin->getPort(i)->getContentType()) {
            case AAP_CONTENT_TYPE_AUDIO:
                if (plugin->getPort(i)->getPortDirection() == AAP_PORT_DIRECTION_INPUT)
                    memcpy(aapBuffer->get_buffer(*aapBuffer, i),
                       audioData->audio.getView().getChannel(currentChannelInAudioData).data.data, numFrames);
                currentChannelInAudioData++;
                break;
            case AAP_CONTENT_TYPE_MIDI2: {
                if (plugin->getPort(i)->getPortDirection() != AAP_PORT_DIRECTION_INPUT)
                    continue;
                size_t midiSize = std::min(aapBuffer->get_buffer_size(*aapBuffer, i),
                                           audioData->midi_capacity);
                memcpy(aapBuffer->get_buffer(*aapBuffer, i), (const void *) audioData->midi_in,
                       midiSize);
                break;
            }
            default:
                break;
        }
    }

    plugin->process(numFrames, 0); // FIXME: timeout?

    currentChannelInAudioData = 0;
    for (int32_t i = 0, n = aapBuffer->num_ports(*aapBuffer); i < n; i++) {
        switch (plugin->getPort(i)->getContentType()) {
            case AAP_CONTENT_TYPE_AUDIO:
                if (plugin->getPort(i)->getPortDirection() == AAP_PORT_DIRECTION_OUTPUT)
                    memcpy(audioData->audio.getView().getChannel(currentChannelInAudioData).data.data,aapBuffer->get_buffer(*aapBuffer, i), numFrames);
                currentChannelInAudioData++;
                break;
            case AAP_CONTENT_TYPE_MIDI2: {
                if (plugin->getPort(i)->getPortDirection() != AAP_PORT_DIRECTION_OUTPUT)
                    continue;
                size_t midiSize = std::min(aapBuffer->get_buffer_size(*aapBuffer, i),
                                           audioData->midi_capacity);
                memcpy(audioData->midi_out, aapBuffer->get_buffer(*aapBuffer, i), midiSize);
                break;
            }
            default:
                break;
        }
    }
}

void aap::AudioPluginNode::start() {
    if (plugin->getInstanceState() == aap::PluginInstantiationState::PLUGIN_INSTANTIATION_STATE_UNPREPARED)
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

aap::MidiSourceNode::MidiSourceNode(AudioGraph* ownerGraph,
                                    RemotePluginInstance* instance,
                                    int32_t sampleRate,
                                    int32_t audioNumFramesPerCallback,
                                    int32_t internalBufferSize) :
        AudioGraphNode(ownerGraph),
        capacity(internalBufferSize),
        translator(instance, internalBufferSize),
        sample_rate(sampleRate),
        aap_frame_size(audioNumFramesPerCallback) {
    buffer = (uint8_t*) calloc(1, internalBufferSize);
}

aap::MidiSourceNode::~MidiSourceNode() {
    free(buffer);
}

void aap::MidiSourceNode::processAudio(AudioData *audioData, int32_t numFrames) {
    auto dstBuffer = (AAPMidiBufferHeader*) audioData->midi_in;
    // MIDI event might be being added, so we wait a bit using "almost-spin" lock (uses nano-sleep).
    if (std::unique_lock<NanoSleepLock> tryLock(midi_buffer_mutex, std::try_to_lock); tryLock.owns_lock()) {
        auto srcBuffer = (AAPMidiBufferHeader*) buffer;
        if (srcBuffer->length)
            memcpy(dstBuffer + 1, srcBuffer + 1, srcBuffer->length);
        dstBuffer->length = srcBuffer->length;
        srcBuffer->length = 0;
    } else {
        // failed to acquire lock; we do not send anything this time.
        dstBuffer->length = 0;
    }
    dstBuffer->time_options = 0; // reserved in MIDI2 mode
}

void aap::MidiSourceNode::addMidiEvent(uint8_t *bytes, int32_t length, int64_t timestampInNanoseconds) {

    // This function is invoked every time Kotlin PluginPlayer.addMidiEvent() is invoked, immediately.
    // On the other hand, AAPs don't process MIDI events immediately (it is handled at `process()`),
    // so we have to buffer them and flush them to the instrument plugin every time process() is invoked.
    //
    // Since we don't really know when it is actually processed but have to accurately
    // pass delta time for each input events, we assign event timecode from
    // 1) argument timestampInNanoseconds and 2) actual time passed from previous call to
    // this function.
    int64_t actualTimestamp = timestampInNanoseconds;
    struct timespec curtime{};
    clock_gettime(CLOCK_REALTIME, &curtime);

    // it is 99.999... percent true since audio loop must have started before any MIDI events...
    if (last_aap_process_time.tv_sec > 0) {
        int64_t diff = (curtime.tv_sec - last_aap_process_time.tv_sec) * 1000000000 +
                       curtime.tv_nsec - last_aap_process_time.tv_nsec;
        auto nanosecondsPerCycle = (int64_t) (1.0 * aap_frame_size / sample_rate * 1000000000);
        actualTimestamp = (timestampInNanoseconds + diff) % nanosecondsPerCycle;
    }

    // apply translation at this step (every time event is added, non-RT processing, unlocked)
    size_t translatedLength = translator.translateMidiEvent(bytes, length);
    auto actualData = translatedLength > 0 ? translator.getTranslationBuffer() : bytes;
    auto actualLength = translatedLength > 0 ? translatedLength : length;

    // update MIDI input buffer with lock
    { // lock scope
        auto dst8 = (uint8_t *) buffer;
        auto dstMBH = (AAPMidiBufferHeader *) dst8;
        const std::lock_guard <NanoSleepLock> lock{midi_buffer_mutex};

        if (dst8 != nullptr) {
            uint32_t savedOffset = dstMBH->length;
            uint32_t currentOffset = savedOffset;
            int32_t tIter = 0;
            auto headerSize = sizeof(AAPMidiBufferHeader);
            for (int64_t ticks = actualTimestamp / (1000000000 / 31250);
                 ticks > 0; ticks -= 31250, tIter++) {
                *(int32_t * )(dst8 + headerSize + currentOffset + tIter * 4) =
                        (int32_t) cmidi2_ump_jr_timestamp_direct(0,
                                                                 ticks > 31250 ? 31250 : ticks);
            }
            currentOffset += tIter * 4;
            memcpy(dst8 + headerSize + currentOffset, actualData, actualLength);
            currentOffset += actualLength;

            if (savedOffset != dstMBH->length) {
                // updated
                memcpy(dst8 + headerSize, dst8 + headerSize + savedOffset, actualLength);
                dstMBH->length = currentOffset - savedOffset;
            } else
                dstMBH->length = currentOffset;
        }
    }
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

