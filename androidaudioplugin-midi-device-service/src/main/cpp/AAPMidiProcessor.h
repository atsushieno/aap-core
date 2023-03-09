
#ifndef AAP_MIDI_DEVICE_SERVICE_AAPMIDIPROCESSOR_H
#define AAP_MIDI_DEVICE_SERVICE_AAPMIDIPROCESSOR_H

#include <vector>
#include <zix/ring.h>
#include <cmidi2.h>
#include <aap/core/host/audio-plugin-host.h>
#include <aap/core/aapxs/extension-service.h>

namespace aapmidideviceservice {
    // keep it compatible with Oboe
    enum AudioCallbackResult : int32_t {
        Continue = 0,
        Stop = 1
    };

    class AAPMidiProcessorPAL {
    public:
        // They are used to control callback-based audio output streaming.
        virtual int32_t setupStream() = 0;
        virtual int32_t startStreaming() = 0;
        virtual int32_t stopStreaming() = 0;
        // It is kind of raw MIDI input event listener (not a "handler" that overrides processing).
        // The Android Stub implementation would override this to dump the messages to Android log.
        virtual void midiInputReceived(uint8_t* bytes, size_t offset, size_t length, int64_t timestampInNanoseconds) {}
    };

    enum AudioDriverType {
        AAP_MIDI_PROCESSOR_AUDIO_DRIVER_TYPE_OBOE,
        AAP_MIDI_PROCESSOR_AUDIO_DRIVER_TYPE_STUB
    };

    enum AAPMidiProcessorState {
        AAP_MIDI_PROCESSOR_STATE_CREATED,
        AAP_MIDI_PROCESSOR_STATE_ACTIVE,
        AAP_MIDI_PROCESSOR_STATE_INACTIVE,
        AAP_MIDI_PROCESSOR_STATE_STOPPED,
        AAP_MIDI_PROCESSOR_STATE_ERROR
    };

    enum AAPMidiTranslationType {
        AAP_MIDI_TRANSLATION_TYPE_NONE,
        AAP_MIDI_TRANSLATION_TYPE_1To2,
        AAP_MIDI_TRANSLATION_TYPE_2To1
    };

    class PluginInstanceData {
        std::vector<int> audio_out_ports{};

    public:
        PluginInstanceData(int instanceId, size_t numPorts) : instance_id(instanceId) {
            auto arr = (void**) calloc(sizeof(void*), numPorts + 1);
            arr[numPorts] = nullptr;
        }
        inline std::vector<int32_t>* getAudioOutPorts() { return &audio_out_ports; }

        int instance_id;
        int midi1_in_port{-1};
        int midi2_in_port{-1};
    };

    class AAPMidiProcessor {
        static std::string convertStateToText(AAPMidiProcessorState state);

        // AAP
        aap::PluginListSnapshot plugin_list{};
        std::unique_ptr<aap::PluginClient> client{nullptr};
        int32_t sample_rate{0};
        int32_t aap_frame_size{4096};
        int32_t midi_buffer_size{4096};
        int32_t channel_count{2};
        std::unique_ptr<PluginInstanceData> instance_data;
        int32_t instrument_instance_id{0};
        // MIDI protocol type of the messages it receives via JNI
        int32_t receiver_midi_protocol{CMIDI2_PROTOCOL_TYPE_MIDI1};
        int32_t current_mapping_policy{AAP_PARAMETERS_MAPPING_POLICY_NONE};

        int32_t getAAPMidiInputPortType();
        PluginInstanceData* getAAPMidiInputData();
        void* getAAPMidiInputBuffer();
        // used when we need MIDI1<->UMP translation.
        uint8_t* translation_buffer{nullptr};

        // If needed, translate MIDI1 bytestream to MIDI2 UMP.
        // returns 0 if translation did not happen. Otherwise return the size of translated buffer in translation_buffer.
        size_t translateMidiBufferIfNeeded(uint8_t* bytes, size_t offset, size_t length);

        // If needed, process MIDI mapping for parameters (CC/ACC/PNACC to sysex8) and presets (program).
        // returns 0 if translation did not happen. Otherwise return the size of translated buffer in translation_buffer.
        size_t runThroughMidi2UmpForMidiMapping(uint8_t* bytes, size_t offset, size_t length);

        // Outputs
        ZixRing *aap_input_ring_buffer{nullptr};
        float *interleave_buffer{nullptr};
        struct timespec last_aap_process_time{};

        // I don't think simple and stupid SpinLock is appropriate here. We do not want to dry up mobile battery.
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
        NanoSleepLock midi_buffer_mutex{};
        uint8_t midi_input_buffer[4096];
        std::atomic<int32_t> midi_input_buffer_size{0};

    protected:
        AAPMidiProcessorState state{AAP_MIDI_PROCESSOR_STATE_CREATED};
        virtual AAPMidiProcessorPAL* pal() = 0;

    public:
        void initialize(aap::PluginClientConnectionList* connections, int32_t sampleRate, int32_t channelCount, int32_t aapFrameSize, int32_t midiBufferSize);

        void instantiatePlugin(std::string pluginId);

        int32_t getInstrumentMidiMappingPolicy();

        void activate();

        void processMidiInput(uint8_t* bytes, size_t offset, size_t length, int64_t timestampInNanoseconds);

        void callPluginProcess();

        void fillAudioOutput();

        void deactivate();

        void terminate();

        int32_t onAudioReady(void *audioData, int32_t numFrames);
    };
}

#endif //AAP_MIDI_DEVICE_SERVICE_AAPMIDIPROCESSOR_H
