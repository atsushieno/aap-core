#ifndef AAP_CORE_PLUGINPLAYER_H
#define AAP_CORE_PLUGINPLAYER_H

#include "PluginPlayerConfiguration.h"
#include "AudioGraph.h"

namespace aap {
    class PluginPlayer {
        PluginPlayerConfiguration configuration;
        SimpleLinearAudioGraph graph;

    public:
        PluginPlayer(PluginPlayerConfiguration &configuration,
                     aap::RemotePluginInstance *instance);
    };
}


#endif //AAP_CORE_PLUGINPLAYER_H
