#include "AudioGraph.h"
#include "AudioGraphNode.h"
#include "LocalDefinitions.h"
#include <thread>

aap::AudioPluginNode::~AudioPluginNode() {
    if (!plugin)
        return;
    plugin->setBusesChangedHandler({});
    plugin->deactivate();
    // The plugin is not disposed here; somewhere that instantiates the plugin should do the job.
}

void aap::AudioPluginNode::setPlugin(RemotePluginInstance* instance) {
    if (plugin)
        plugin->setBusesChangedHandler({});
    plugin = instance;
    if (plugin)
        plugin->setBusesChangedHandler([this](uint32_t flags) { onBusesChanged(flags); });
}

// Invoked on the extension worker, or in setPlugin() for earlier changes: prepare the plugin
// again for its new bus layout. An unprepared plugin only refreshes it; start() prepares it.
void aap::AudioPluginNode::onBusesChanged(uint32_t flags) {
    if (!(flags & AAP_BUSES_CHANGED_LAYOUT) || !plugin)
        return;
    reconfiguring = true;
    while (processing.load() != 0)
        std::this_thread::yield();
    auto state = plugin->getInstanceState();
    bool wasActive = state == PLUGIN_INSTANTIATION_STATE_ACTIVE;
    bool wasPrepared = wasActive || state == PLUGIN_INSTANTIATION_STATE_INACTIVE;
    plugin->deactivate();
    // Keep the event buffer size that the instance was prepared with.
    auto buffer = plugin->getAudioPluginBuffer();
    auto eventIn = plugin->getMainEventPortIndex(AAP_PORT_DIRECTION_INPUT);
    auto controlBytes = buffer && eventIn >= 0 ? buffer->get_buffer_size(buffer, eventIn) : DEFAULT_CONTROL_BUFFER_SIZE;
    auto error = plugin->refreshBusLayout();
    if (error.empty() && wasPrepared) {
        plugin->prepare(graph->getFramesPerCallback(), graph->getSampleRate(), controlBytes);
        if (wasActive)
            plugin->activate();
    } else if (!error.empty())
        aap::a_log_f(AAP_LOG_LEVEL_ERROR, AAP_MANAGER_LOG_TAG, "Failed to refresh the bus layout: %s", error.c_str());
    reconfiguring = false;
}

bool aap::AudioPluginNode::shouldSkip() {
    return plugin == nullptr;
}

void aap::AudioPluginNode::processAudio(AudioBuffer *audioData, int32_t numFrames) {
    if (!plugin)
        return;
    processing++;
    struct ProcessingScope { std::atomic<int32_t>& count; ~ProcessingScope() { count--; } } scope{processing};
    if (reconfiguring.load())
        return;

    // Copy input audioData into each plugin's buffer (it is inevitable; each plugin has
    // shared memory between the service and this host, which are not sharable with other plugins
    // in the chain. So, it's optimal enough.)

    auto aapBuffer = plugin->getAudioPluginBuffer();

    // So far the graph routes only the main buses.
    auto audioIn = plugin->getBus(AAP_BUS_KIND_AUDIO, AAP_PORT_DIRECTION_INPUT, 0);
    auto numChannels = (int32_t) audioData->audio.getNumChannels();
    for (int32_t ch = 0, n = audioIn ? std::min(audioIn->getChannelCount(), numChannels) : 0; ch < n; ch++)
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
    for (int32_t ch = 0, n = audioOut ? std::min(audioOut->getChannelCount(), numChannels) : 0; ch < n; ch++)
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
