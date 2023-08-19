#include "PluginPlayer.h"
#include <aap/core/host/plugin-instance.h>

aap::PluginPlayer::PluginPlayer(aap::PluginPlayerConfiguration &pluginPlayerConfiguration,
                                aap::RemotePluginInstance *instance) :
                                configuration(pluginPlayerConfiguration),
                                graph(configuration.getFramesPerCallback(), instance) {
}
