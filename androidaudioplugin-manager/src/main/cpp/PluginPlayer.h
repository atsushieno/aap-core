#ifndef AAP_CORE_PLUGINPLAYER_H
#define AAP_CORE_PLUGINPLAYER_H

#include "PluginPlayerConfiguration.h"
#include "AudioGraph.h"

namespace aap {
    class PluginPlayer {
        PluginPlayerConfiguration configuration;

    public:
        PluginPlayer(PluginPlayerConfiguration &configuration);

        SimpleLinearAudioGraph graph;

        void setAudioSource(uint8_t *data, int dataLength, const char *filename);

        void startProcessing();

        void pauseProcessing();
    };
}


#endif //AAP_CORE_PLUGINPLAYER_H
