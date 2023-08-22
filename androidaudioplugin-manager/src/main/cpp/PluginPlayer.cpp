#include "PluginPlayer.h"
#include <aap/core/host/plugin-instance.h>

aap::PluginPlayer::PluginPlayer(aap::PluginPlayerConfiguration &pluginPlayerConfiguration) :
                                configuration(pluginPlayerConfiguration),
                                graph(configuration.getFramesPerCallback()) {
}

void aap::PluginPlayer::setAudioSource(uint8_t *data, int32_t dataLength, const char *filename) {
    // TODO: implement uncompressing `data` into AAP audio data.
}

void aap::PluginPlayer::startProcessing() {
    graph.startProcessing();
}

void aap::PluginPlayer::pauseProcessing() {
    graph.pauseProcessing();
}

void aap::PluginPlayer::enableAudioRecorder() {
    graph.enableAudioRecorder();
}
