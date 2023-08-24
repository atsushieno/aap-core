#ifndef AAP_CORE_AUDIODATA_H
#define AAP_CORE_AUDIODATA_H

#include "LocalDefinitions.h"
#define AAP_MANAGER_AUDIO_QUEUE_NX_FRAMES 4
#include <audio/choc_SampleBuffers.h>
#include <containers/choc_SingleReaderSingleWriterFIFO.h>

namespace aap {

    struct AudioData {
        choc::buffer::ChannelArrayBuffer<float> audio;
        choc::fifo::SingleReaderSingleWriterFIFO<float> midi_in;
        choc::fifo::SingleReaderSingleWriterFIFO<float> midi_out;

        AudioData(int32_t numChannels, int32_t framesPerCallback) {
            audio = choc::buffer::createChannelArrayBuffer(numChannels, framesPerCallback * AAP_MANAGER_AUDIO_QUEUE_NX_FRAMES, []() { return (float) 0; });
        }
    };

}

#endif //AAP_CORE_AUDIODATA_H
