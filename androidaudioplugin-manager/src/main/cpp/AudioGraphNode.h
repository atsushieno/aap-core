#ifndef AAP_CORE_AUDIOGRAPHNODE_H
#define AAP_CORE_AUDIOGRAPHNODE_H

#include "AudioDevice.h"
#include "AAPMidiEventTranslator.h"
#include <aap/core/host/plugin-instance.h>
#include <cmidi2.h>

namespace aap {
    class AudioGraph;

    class AudioGraphNode {

    protected:
        AudioGraph* graph;

        AudioGraphNode(AudioGraph* ownerGraph) : graph(ownerGraph) {}

    public:

        virtual bool shouldSkip() { return false; }
        virtual void start() = 0;
        virtual void pause() = 0;
        virtual void processAudio(AudioData* audioData, int32_t numFrames) = 0;
    };

    /**
     * AudioDeviceInputNode outputs the audio input from the device callback at processing.
     *
     * Single producer, multiple consumer: more than one node could retrieve the same input buffer.
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
        bool permission_granted{false};

    public:
        AudioDeviceInputNode(AudioGraph* ownerGraph, AudioDeviceIn* input) :
                AudioGraphNode(ownerGraph),
                input(input) {
        }

        AudioDeviceIn* getDevice() { return input; }

        bool shouldSkip() override;

        void start() override;
        void pause() override;
        void processAudio(AudioData* audioData, int32_t numFrames) override;

        void setPermissionGranted();
    };

    /**
     * AudioDeviceOutputNode outputs its input directly to the device at processing.
     *
     * Single producer (app), single consumer (audio device callback).
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
        AudioDeviceOutputNode(AudioGraph* ownerGraph, AudioDeviceOut* output) :
                AudioGraphNode(ownerGraph),
                output(output) {
        }

        AudioDeviceOut* getDevice() { return output; }

        void start() override;
        void pause() override;
        void processAudio(AudioData* audioData, int32_t numFrames) override;
    };

    class AudioPluginNode : public AudioGraphNode {
        RemotePluginInstance* plugin;

    public:
        AudioPluginNode(AudioGraph* ownerGraph, RemotePluginInstance* plugin) :
                AudioGraphNode(ownerGraph),
                plugin(plugin) {
        }

        void setPlugin(RemotePluginInstance* instance) { plugin = instance; }

        void start() override;
        void pause() override;
        bool shouldSkip() override;
        void processAudio(AudioData* audioData, int32_t numFrames) override;
    };

    AAP_OPEN_CLASS class AudioSourceNode : public AudioGraphNode {
        bool active{false};
        bool playing{false};

    public:
        AudioSourceNode(AudioGraph* ownerGraph) :
            AudioGraphNode(ownerGraph) {
        }

        void start() override;
        void pause() override;
        bool shouldSkip() override;
        void processAudio(AudioData* audioData, int32_t numFrames) override;

        virtual bool hasData() { return false; };

        void setPlaying(bool newPlayingState);

        virtual int32_t read(AudioData* dst, int32_t numFrames) = 0;
    };

    class AudioDataSourceNode : public AudioSourceNode {
        void* audio_data{nullptr};
        int32_t frames{0};
        int32_t channels{0};

        int32_t current_offset{0};

    public:
        explicit AudioDataSourceNode(AudioGraph* ownerGraph) :
                AudioSourceNode(ownerGraph) {
        }

        void setData(AudioData* audioData, int32_t numFrames, int32_t numChannels);

        int32_t read(AudioData* dst, int32_t numFrames) override;
    };

    // I don't think any simple and stupid SpinLock works well on mobiles, as we do not want to dry up battery.
    // FIXME: unify code with androidaudioplugin
    class NanoSleepLock {
        std::atomic_flag state = ATOMIC_FLAG_INIT;
    public:
        void lock() noexcept {
            const auto delay = timespec{0, 1000}; // 1 microsecond
            while(state.test_and_set())
                clock_nanosleep(CLOCK_REALTIME, 0, &delay, nullptr);
        }
        void unlock() noexcept { state.clear(); }
        bool try_lock() noexcept { return !state.test_and_set(); }
    };


    class MidiSourceNode : public AudioGraphNode {
        uint8_t* buffer;
        int32_t capacity;
        int32_t consumer_offset{0};
        int32_t producer_offset{0};

        NanoSleepLock midi_buffer_mutex{};
        AAPMidiEventTranslator translator;

        // needed for sample accurate time calculation
        int32_t sample_rate{0};
        int32_t aap_frame_size{1024};
        struct timespec last_aap_process_time{0, 0};

    public:
        MidiSourceNode(AudioGraph* ownerGraph, RemotePluginInstance* instance, int32_t sampleRate, int32_t audioNumFramesPerCallback, int32_t internalBufferSize = AAP_PLUGIN_PLAYER_DEFAULT_MIDI_RING_BUFFER_SIZE);

        virtual ~MidiSourceNode();

        void addMidiEvent(uint8_t *data, int32_t length, int64_t timestampInNanoseconds);

        void start() override;
        void pause() override;
        void processAudio(AudioData* audioData, int32_t numFrames) override;
    };

    class MidiDestinationNode : public AudioGraphNode {
        uint8_t* buffer;
        int32_t consumer_offset{0};

    public:
        MidiDestinationNode(AudioGraph* ownerGraph, int32_t internalBufferSize = AAP_PLUGIN_PLAYER_DEFAULT_MIDI_RING_BUFFER_SIZE);

        virtual ~MidiDestinationNode();

        void start() override;
        void pause() override;
        void processAudio(AudioData* audioData, int32_t numFrames) override;
    };
}



#endif //AAP_CORE_AUDIOGRAPHNODE_H
