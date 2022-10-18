#ifndef AAP_CORE_AAP_SAMPLE_HOST_ENGINE_H
#define AAP_CORE_AAP_SAMPLE_HOST_ENGINE_H

#include <vector>
#include <zix/ring.h>
#include <cmidi2.h>
#include <aap/core/host/audio-plugin-host.h>
#include <aap/core/aapxs/extension-service.h>

namespace aap::host_engine {

#define LOG_TAG "AAPSampleHostEngine2"

// keep it compatible with Oboe
enum AudioCallbackResult : int32_t {
    Continue = 0,
    Stop = 1
};

class AAPHostEnginePAL {
public:
    virtual int32_t setupStream() = 0;
    virtual int32_t startStreaming() = 0;
    virtual int32_t stopStreaming() = 0;
    virtual void midiInputReceived(uint8_t* bytes, size_t offset, size_t length, int64_t timestampInNanoseconds) {}
};

enum AudioDriverType {
    AAP_HOST_ENGINE_AUDIO_DRIVER_TYPE_OBOE,
    AAP_HOST_ENGINE_AUDIO_DRIVER_TYPE_STUB
};

enum AAPHostEngineState {
    AAP_HOST_ENGINE_STATE_CREATED,
    AAP_HOST_ENGINE_STATE_ACTIVE,
    AAP_HOST_ENGINE_STATE_INACTIVE,
    AAP_HOST_ENGINE_STATE_STOPPED,
    AAP_HOST_ENGINE_STATE_ERROR
};

class PluginInstanceData {
    std::vector<int> audio_out_ports{};

public:
    PluginInstanceData(int instanceId, size_t numPorts) : instance_id(instanceId) {
        auto arr = (void**) calloc(sizeof(void*), numPorts + 1);
        arr[numPorts] = nullptr;
        buffer_pointers.reset(arr);
    }
    inline std::vector<int32_t>* getAudioOutPorts() { return &audio_out_ports; }

    int instance_id;
    int midi1_in_port{-1};
    int midi2_in_port{-1};
    AndroidAudioPluginBuffer* plugin_buffer;
    std::unique_ptr<void*> buffer_pointers{nullptr};
};

class AAPHostEngineBase {
    static std::string convertStateToText(AAPHostEngineState state);

    // AAP
    aap::PluginListSnapshot plugin_list{};
    std::unique_ptr<aap::PluginClient> client{nullptr};
    int sample_rate{0};
    int aap_frame_size{4096};
    int channel_count{2};
    std::unique_ptr<PluginInstanceData> instance_data;
    int instrument_instance_id{0};
    int32_t midi_protocol{CMIDI2_PROTOCOL_TYPE_MIDI1};

    void* getAAPMidiInputBuffer();

    // Outputs
    ZixRing *aap_input_ring_buffer{nullptr};
    float *interleave_buffer{nullptr};
    struct timespec last_aap_process_time{};

protected:
    AAPHostEngineState state{AAP_HOST_ENGINE_STATE_CREATED};
    virtual AAPHostEnginePAL* pal() = 0;

public:
    void initialize(aap::PluginClientConnectionList* connections, int32_t sampleRate, int32_t channelCount, int32_t aapFrameSize);

    void instantiatePlugin(std::string pluginId);

    void activate();

    void processMidiInput(uint8_t* bytes, size_t offset, size_t length, int64_t timestampInNanoseconds);

    void callPluginProcess();

    void fillAudioOutput();

    void deactivate();

    void terminate();

    int32_t onAudioReady(void *audioData, int32_t numFrames);
};

} // end of namespaces

#endif //AAP_CORE_AAP_SAMPLE_HOST_ENGINE_H
