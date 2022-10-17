/*

Inputs:

    MIDI sources
        void* smf;
        uint32_t* umpSequence;

    Audio sources
        void* wav;
        void** buffer;

    Audio Plugin inputs
        AndroidAudioPluginBuffer *buffer;

Outputs:

    MIDI sequence
        uint32_t* umpSequence;

Features:

    - extract audio channel buffers from wav
    - output wav from audio channel buffers
    - translate SMF into UMP

*/


#include <aap/core/host/audio-plugin-host.h>

namespace aap::samples::host_engine {

class MusicPlaybackInput {
public:
    uint8_t *smf;
    uint32_t *umpSequence;
    void *wav;
};


class MusicPlayback {
public:
    enum Result {
        OK
    };

    Result processStatic() {
        throw std::runtime_error("TODO"); // FIXME: implement
    }
};

class MidiBuffer {
    uint32_t *ump;
    int32_t size;
};

class AudioBuffer {
    float **data;
    int32_t channel_count;
};

class AAPApplyProcessData {
public:
    MidiBuffer midi_in;
    MidiBuffer midi_out;
    AudioBuffer **audio_buffers;
    int32_t frame_count;
};


class AAPApplyAudioProcessor {
    void process(AAPApplyProcessData &data) {
        throw std::runtime_error("TODO"); // FIXME: implement
    }
};


} // end of namespace decls.
