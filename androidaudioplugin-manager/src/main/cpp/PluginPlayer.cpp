#include "PluginPlayer.h"
#include <aap/core/host/plugin-instance.h>

aap::PluginPlayer::PluginPlayer(aap::PluginPlayerConfiguration &pluginPlayerConfiguration) :
                                configuration(pluginPlayerConfiguration),
                                graph(configuration.getFramesPerCallback()) {
}

void aap::PluginPlayer::setAudioSource(uint8_t *data, int32_t dataLength, const char *filename) {
    // TODO: implement
}

void aap::PluginPlayer::startProcessing() {
    // TODO: implement
}

void aap::PluginPlayer::pauseProcessing() {
    // TODO: implement
}
