#include "AudioData.h"

aap::AudioData::AudioData(int32_t numChannels, int32_t framesPerCallback, int32_t midiBufferSize) {
    audio = choc::buffer::createChannelArrayBuffer(numChannels,
                                                   framesPerCallback * AAP_MANAGER_AUDIO_QUEUE_NX_FRAMES,
                                                   []() { return (float) 0; });
    audio.clear();
    midi_capacity = midiBufferSize;
    midi_in = calloc(1, midiBufferSize);
    midi_out = calloc(1, midiBufferSize);
}


aap_buffer_t aap::AudioData::asAAPBuffer() {
    aap_buffer_t ret{};
    ret.impl = this;
    ret.num_frames = aapBufferGetNumFrames;
    ret.get_buffer = aapBufferGetBuffer;
    ret.get_buffer_size = aapBufferGetBufferSize;
    ret.num_ports = aapBufferGetNumPorts;
    return ret;
}

int32_t aap::AudioData::aapBufferGetNumFrames(aap_buffer_t &b) {
    return ((AudioData*) b.impl)->audio.getNumFrames();
}

void *aap::AudioData::aapBufferGetBuffer(aap_buffer_t &b, int32_t portIndex) {
    auto data = (AudioData*) b.impl;
    if (portIndex < data->audio.getChannelRange().end)
        return data->audio.getChannel(portIndex).data.data;
    switch (portIndex - data->audio.getChannelRange().end) {
        case 0: return data->midi_in;
        case 1: return data->midi_out;
    }
    assert(false); // should not reach here
}

int32_t aap::AudioData::aapBufferGetBufferSize(aap_buffer_t &b, int32_t portIndex) {
    auto data = (AudioData*) b.impl;
    if (portIndex < data->audio.getChannelRange().end)
        return data->audio.getNumFrames() * sizeof(float);
    switch (portIndex - data->audio.getChannelRange().end) {
        case 0:
        case 1:
            return data->midi_capacity;
    }
    assert(false); // should not reach here
}

int32_t aap::AudioData::aapBufferGetNumPorts(aap_buffer_t &b) {
    auto data = (AudioData*) b.impl;
    return data->audio.getNumChannels() + 2;
}
