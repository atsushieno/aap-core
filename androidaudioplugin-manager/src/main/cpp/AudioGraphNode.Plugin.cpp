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

    // So far the graph routes only the main buses.
    auto audioIn = plugin->getBus(AAP_BUS_KIND_AUDIO, AAP_PORT_DIRECTION_INPUT, 0);
    for (int32_t ch = 0, n = audioIn ? audioIn->getChannelCount() : 0; ch < n; ch++)
        memcpy(aapBuffer->get_buffer(aapBuffer, audioIn->getPortIndex(ch)),
               audioData->audio.getView().getChannel(ch).data.data,
               numFrames * sizeof(float));
    auto midiIn = plugin->getMainEventPortIndex(AAP_PORT_DIRECTION_INPUT);
    if (midiIn >= 0) {
        auto mbh = (AAPMidiBufferHeader*) audioData->midi_in;
        size_t midiSize = std::min((int32_t) (sizeof(AAPMidiBufferHeader) + mbh->length),
                                   std::min(aapBuffer->get_buffer_size(aapBuffer, midiIn), audioData->midi_capacity));
        memcpy(aapBuffer->get_buffer(aapBuffer, midiIn), (const void *) audioData->midi_in, midiSize);
    }

    plugin->process(numFrames, 0); // FIXME: timeout?

    auto audioOut = plugin->getBus(AAP_BUS_KIND_AUDIO, AAP_PORT_DIRECTION_OUTPUT, 0);
    for (int32_t ch = 0, n = audioOut ? audioOut->getChannelCount() : 0; ch < n; ch++)
        memcpy(audioData->audio.getView().getChannel(ch).data.data,
               aapBuffer->get_buffer(aapBuffer, audioOut->getPortIndex(ch)),
               numFrames * sizeof(float));
    auto midiOut = plugin->getMainEventPortIndex(AAP_PORT_DIRECTION_OUTPUT);
    if (midiOut >= 0) {
        size_t midiSize = std::min(aapBuffer->get_buffer_size(aapBuffer, midiOut),
                                   audioData->midi_capacity);
        auto* midiBuffer = aapBuffer->get_buffer(aapBuffer, midiOut);
        memcpy(audioData->midi_out, midiBuffer, midiSize);
        ((AAPMidiBufferHeader*) midiBuffer)->length = 0;
    }
}

void aap::AudioPluginNode::start() {
    if (plugin->getInstanceState() == aap::PluginInstantiationState::PLUGIN_INSTANTIATION_STATE_UNPREPARED)
        plugin->prepare(graph->getFramesPerCallback(), graph->getSampleRate());
    plugin->activate();
}

void aap::AudioPluginNode::pause() {
    plugin->deactivate();
}

void aap::AudioPluginNode::setPresetIndex(int32_t index) {
    plugin->getStandardExtensions().setCurrentPresetIndex(index);
}
