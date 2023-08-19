#ifndef AAP_CORE_AUDIOGRAPH_H
#define AAP_CORE_AUDIOGRAPH_H

#include <cstdint>
#include <cassert>

#include "LocalDefinitions.h"
#include "AudioDeviceManager.h"
#include "AudioGraphNode.h"

namespace aap {
    AAP_OPEN_CLASS class AudioGraph {
    public:
        virtual void processAudio(void* audioData, int32_t numFrames) = 0;
    };

    class SimpleLinearAudioGraph : public AudioGraph {
        //AudioDeviceInputNode input;
        AudioDeviceOutputNode output;
        AudioPluginNode plugin;
        AudioSourceNode file;

        static void audio_callback(void* callbackContext, void* audioData, int32_t numFrames) {
            ((SimpleLinearAudioGraph*) callbackContext)->processAudio(audioData, numFrames);
        }

    public:
        SimpleLinearAudioGraph(uint32_t framesPerCallback, RemotePluginInstance* instance) :
                //input(AudioDeviceManager::getInstance()->openDefaultInput(framesPerCallback)),
                output(AudioDeviceManager::getInstance()->openDefaultOutput(framesPerCallback)),
                plugin(instance),
                file() {
            output.getDevice()->setAudioCallback(audio_callback);
        }
    };

    class AudioGraphNode;

    class BasicAudioGraph : public AudioGraph {
    public:
        BasicAudioGraph() {
            assert(false); // TODO
        }

        void attachNode(AudioGraphNode* sourceNode, int32_t sourceOutputBusIndex, AudioGraphNode* destinationNode, int32_t destinationInputBusIndex);
        void detachNode(AudioGraphNode* sourceNode, int32_t sourceOutputBusIndex);
    };
}

#endif //AAP_CORE_AUDIOGRAPH_H
