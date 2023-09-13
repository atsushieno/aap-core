#include "AudioGraph.h"
#include "AudioGraphNode.h"

aap::AudioPluginNode::~AudioPluginNode() {
    plugin->deactivate();
    // The plugin is not disposed here; somewhere that instantiates the plugin should do the job.
}

bool aap::AudioPluginNode::shouldSkip() {
    return plugin == nullptr;
}

void aap::AudioPluginNode::processAudio(AudioBuffer *audioData, int32_t numFrames) {
    if (!plugin)
        return;

    // Copy input audioData into each plugin's buffer (it is inevitable; each plugin has
    // shared memory between the service and this host, which are not sharable with other plugins
    // in the chain. So, it's optimal enough.)

    auto aapBuffer = plugin->getAudioPluginBuffer();

    int32_t currentChannelInAudioData = 0;
    for (int32_t i = 0, n = aapBuffer->num_ports(*aapBuffer); i < n; i++) {
        if (plugin->getPort(i)->getPortDirection() != AAP_PORT_DIRECTION_INPUT)
            continue;
        switch (plugin->getPort(i)->getContentType()) {
            case AAP_CONTENT_TYPE_AUDIO:
                memcpy(aapBuffer->get_buffer(*aapBuffer, i),
                       audioData->audio.getView().getChannel(currentChannelInAudioData).data.data,
                       numFrames * sizeof(float));
                currentChannelInAudioData++;
                break;
            case AAP_CONTENT_TYPE_MIDI2: {
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
        if (plugin->getPort(i)->getPortDirection() != AAP_PORT_DIRECTION_OUTPUT)
            continue;
        switch (plugin->getPort(i)->getContentType()) {
            case AAP_CONTENT_TYPE_AUDIO:
                memcpy(audioData->audio.getView().getChannel(currentChannelInAudioData).data.data,
                       aapBuffer->get_buffer(*aapBuffer, i),
                       numFrames * sizeof(float));
                currentChannelInAudioData++;
                break;
            case AAP_CONTENT_TYPE_MIDI2: {
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

const int32_t OPCODE_SET_PRESET_INDEX = 4;
uint32_t set_preset_sysex8[32];
uint8_t set_preset_helper[128];
void aap::AudioPluginNode::setPresetIndex(int32_t index) {
    if (plugin->getInstanceState() == aap::PluginInstantiationState::PLUGIN_INSTANTIATION_STATE_ACTIVE) {
        // FIXME: this is a temporary implementation until we have valid UMP wrapper around AAPXSClientService.
        memset(set_preset_sysex8, 0, 128);
        int32_t data[]{OPCODE_SET_PRESET_INDEX, index};
        size_t size = aap_midi2_generate_aapxs_sysex8(
                set_preset_sysex8, 32,
                set_preset_helper, 128,
                0,
                AAP_PRESETS_EXTENSION_URI,
                (uint8_t*) data, sizeof(int32_t) * 2);
        plugin->addEventUmpInput((void*) set_preset_sysex8, size);
    }
    else
        plugin->getStandardExtensions().setCurrentPresetIndex(index);
}

