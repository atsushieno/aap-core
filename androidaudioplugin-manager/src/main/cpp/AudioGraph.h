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
        AudioDeviceInputNode input;
        AudioDeviceOutputNode output;
        AudioPluginNode plugin;
        AudioDataSourceNode audio_data;
        MidiSourceNode midi_input;
        MidiDestinationNode midi_output;

        static void audio_callback(void* callbackContext, void* audioData, int32_t numFrames) {
            ((SimpleLinearAudioGraph*) callbackContext)->processAudio(audioData, numFrames);
        }

    public:
        SimpleLinearAudioGraph(uint32_t framesPerCallback) :
                input(AudioDeviceManager::getInstance()->openDefaultInput(framesPerCallback)),
                output(AudioDeviceManager::getInstance()->openDefaultOutput(framesPerCallback)),
                plugin(nullptr),
                audio_data(nullptr, 0, 0),
                midi_input(AAP_PLUGIN_PLAYER_DEFAULT_MIDI_RING_BUFFER_SIZE),
                midi_output(AAP_PLUGIN_PLAYER_DEFAULT_MIDI_RING_BUFFER_SIZE) {
            output.getDevice()->setAudioCallback(audio_callback);
        }

        void setPlugin(RemotePluginInstance* instance);

        void setAudioData(void* data, int32_t numFrames, int32_t numChannels);

        void processAudio(void *audioData, int32_t numFrames) override;

        void addMidiEvent(uint8_t *string, int32_t i);

        void playAudioData();
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
