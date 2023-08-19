#ifndef AAP_CORE_AUDIOGRAPHNODE_H
#define AAP_CORE_AUDIOGRAPHNODE_H

#include "AudioDevice.h"
#include <aap/core/host/plugin-instance.h>

namespace aap {
    class AudioGraphNode {
    public:
        virtual void processAudio(void* audioData, int32_t numFrames) = 0;
    };

    class AudioDeviceInputNode : public AudioGraphNode {
        AudioDeviceIn* input;

    public:
        AudioDeviceInputNode(AudioDeviceIn* input) :
                input(input) {
        }

        AudioDeviceIn* getDevice() { return input; }

        void processAudio(void* audioData, int32_t numFrames) override;
    };

    class AudioDeviceOutputNode : public AudioGraphNode {
        AudioDeviceOut* output;

    public:
        AudioDeviceOutputNode(AudioDeviceOut* output) :
                output(output) {
        }

        AudioDeviceOut* getDevice() { return output; }

        void processAudio(void* audioData, int32_t numFrames) override;
    };

    class AudioPluginNode : public AudioGraphNode {
        RemotePluginInstance* plugin;

    public:
        AudioPluginNode(RemotePluginInstance* plugin) :
                plugin(plugin) {
        }

        void processAudio(void* audioData, int32_t numFrames) override;
    };

    class AudioSourceNode : public AudioGraphNode {

    public:
        AudioSourceNode() {
        }

        void processAudio(void* audioData, int32_t numFrames) override;

    };

    class MidiSourceNode : public AudioGraphNode {

    };

    class MidiDestinationNode : public AudioGraphNode {

    };

}



#endif //AAP_CORE_AUDIOGRAPHNODE_H
