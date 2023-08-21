#ifndef AAP_CORE_AUDIOGRAPHNODE_H
#define AAP_CORE_AUDIOGRAPHNODE_H

#include "AudioDevice.h"
#include <aap/core/host/plugin-instance.h>

#define AAP_PLUGIN_PLAYER_DEFAULT_MIDI_RING_BUFFER_SIZE 8192

namespace aap {
    class AudioGraphNode {
    public:
        virtual bool shouldSkip() { return false; }
        virtual void processAudio(void* audioData, int32_t numFrames) = 0;
    };

    /**
     * AudioDeviceInputNode outputs the audio input from the device callback at processing.
     * More than one node could retrieve the same input buffer.
     *
     * It internally uses ring buffer for audio inputs. (TODO)
     *
     * In case of failed audio processing, it may resend the same output chunk.
     * It does not check if the input was already consumed or not.
     *
     */
    class AudioDeviceInputNode : public AudioGraphNode {
        AudioDeviceIn* input;
        int32_t consumer_position{0};

    public:
        AudioDeviceInputNode(AudioDeviceIn* input) :
                input(input) {
        }

        AudioDeviceIn* getDevice() { return input; }

        void processAudio(void* audioData, int32_t numFrames) override;
    };

    /**
     * AudioDeviceOutputNode outputs its input directly to the device at processing.
     *
     * On a live audio processor, the Oboe audio device callback would kick the overall
     * AudioGraph processing, and this node is typically laid out at the end of one of the chains.
     *
     * AudioGraph itself does not handle output copying - it might just store the processing
     * results on an AudioDestinationNode and let the client app retrieve the buffer.
     */
    class AudioDeviceOutputNode : public AudioGraphNode {
        AudioDeviceOut* output;
        int32_t consumer_position{0};

    public:
        AudioDeviceOutputNode(AudioDeviceOut* output) :
                output(output) {
        }

        AudioDeviceOut* getDevice() { return output; }

        void processAudio(void* audioData, int32_t numFrames) override;
    };

    class AudioPluginNode : public AudioGraphNode {
        RemotePluginInstance* plugin;

    public:
        AudioPluginNode(RemotePluginInstance* plugin) :
                plugin(plugin) {
        }

        void setPlugin(RemotePluginInstance* instance) { plugin = instance; }

        bool shouldSkip() override;
        void processAudio(void* audioData, int32_t numFrames) override;
    };

    AAP_OPEN_CLASS class AudioSourceNode : public AudioGraphNode {

    public:
        AudioSourceNode() {
        }

        bool shouldSkip() override;
        void processAudio(void* audioData, int32_t numFrames) override;

        virtual bool hasData() { return false; };

        virtual int32_t read(void* dst, int32_t size) = 0;
    };

    class AudioDataSourceNode : public AudioSourceNode {
        void* audio_data{nullptr};
        int32_t frames{0};
        int32_t channels{0};
        int32_t current_offset{0};
        bool playing{false};

    public:
        explicit AudioDataSourceNode() {}
        AudioDataSourceNode(void* audioData, int32_t numFrames, int32_t numChannels) {
            setData(audioData, numFrames, numChannels);
        }

        void setData(void* audioData, int32_t numFrames, int32_t numChannels);

        int32_t read(void* dst, int32_t size) override;

        void setPlaying(bool newPlayingState);
    };

    class MidiSourceNode : public AudioGraphNode {
        uint8_t* data;
        int32_t current_play_offset{0};
        int32_t next_data_offset{0};

    public:
        MidiSourceNode(int32_t internalBufferSize = AAP_PLUGIN_PLAYER_DEFAULT_MIDI_RING_BUFFER_SIZE);

        virtual ~MidiSourceNode();

        void addMidiEvent(uint8_t *data, int32_t length);

        void processAudio(void* audioData, int32_t numFrames) override;
    };

    class MidiDestinationNode : public AudioGraphNode {
        uint8_t* data;
        int32_t current_offset{0};

    public:
        MidiDestinationNode(int32_t internalBufferSize = AAP_PLUGIN_PLAYER_DEFAULT_MIDI_RING_BUFFER_SIZE);

        virtual ~MidiDestinationNode();

        void processAudio(void* audioData, int32_t numFrames) override;
    };

}



#endif //AAP_CORE_AUDIOGRAPHNODE_H
