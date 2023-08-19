/*
#ifndef AAP_CORE_AUDIOPROCESSOR_H
#define AAP_CORE_AUDIOPROCESSOR_H

#include "AudioProcessor.h"
#include "AudioDevicePAL.h"
#include <aap/core/host/audio-plugin-host.h>
//#include "containers/choc_SingleReaderSingleWriterFIFO.h"


namespace aap {

    enum AudioProcessorState {
        AAP_MIDI_PROCESSOR_STATE_CREATED,
        AAP_MIDI_PROCESSOR_STATE_ACTIVE,
        AAP_MIDI_PROCESSOR_STATE_INACTIVE,
        AAP_MIDI_PROCESSOR_STATE_STOPPED,
        AAP_MIDI_PROCESSOR_STATE_ERROR
    };

    class AudioProcessorConfiguration {
    public:
        int audio_input_bus_count{1};
        int audio_output_bus_count{1};
    };

    class AudioProcessor {
        int32_t sample_rate{0};
        int32_t buffer_size_in_samples{1024};
        int32_t midi_buffer_size{4096};
        int32_t audio_in_channel_count{1};
        int32_t audio_out_channel_count{2};

    protected:
        AudioProcessorState state{AAP_MIDI_PROCESSOR_STATE_CREATED};
        virtual AudioDevicePAL* pal() = 0;

    public:
        // FIXME: we want to move this sampleRate argument to somewhere later
        void initialize(int32_t sampleRate, int32_t audioInChannelCount, int32_t audioOutChannelCount, int32_t bufferSizeInSamples, int32_t midiBufferSize);

        void instantiatePlugin(std::string pluginId);

        virtual void activate() = 0;

        virtual void deactivate() = 0;

        virtual void terminate() = 0;

        virtual int32_t processAudioIO(void *audioData, int32_t numFrames) = 0;

        inline int32_t getBufferSizeInSamples() { return buffer_size_in_samples; }

        inline int32_t getAudioInChannelCount() { return audio_in_channel_count; }

        inline int32_t getAudioOutChannelCount() { return audio_out_channel_count; }
    };

    class AudioGraphProcessor : public AudioProcessor {

    };
}


#endif //AAP_CORE_AUDIOPROCESSOR_H
*/
