#ifndef AAP_CORE_AUDIOGRAPH_H
#define AAP_CORE_AUDIOGRAPH_H

#include <cstdint>
#include <cassert>

#include "LocalDefinitions.h"
#include "AudioDeviceManager.h"
#include "AudioGraphNode.h"

namespace aap {
    AAP_OPEN_CLASS class AudioGraph {
        int32_t frames_per_callback;
        int32_t num_channels;

    public:
        static int32_t getAAPChannelCount(int32_t channelsInAudioBus) { return channelsInAudioBus + 2; }

        explicit AudioGraph(int32_t framesPerCallback, int32_t channelsInAudioBus) :
                frames_per_callback(framesPerCallback),
                num_channels(channelsInAudioBus) {
        }

        virtual void processAudio(void* audioData, int32_t numFrames) = 0;

        int32_t getFramesPerCallback() { return frames_per_callback; }

        int32_t getChannelsInAudioBus() { return num_channels; }
    };

    class SimpleLinearAudioGraph : public AudioGraph {
        AudioDeviceInputNode input;
        AudioDeviceOutputNode output;
        AudioPluginNode plugin;
        AudioDataSourceNode audio_data;
        MidiSourceNode midi_input;
        MidiDestinationNode midi_output;
        std::vector<AudioGraphNode*> nodes{};

        static void audio_callback(void* callbackContext, AudioData* audioData, int32_t numFrames) {
            ((SimpleLinearAudioGraph*) callbackContext)->processAudio(audioData, numFrames);
        }

    public:
        SimpleLinearAudioGraph(uint32_t framesPerCallback, int32_t channelsInAudioBus);

        void setPlugin(RemotePluginInstance* instance);

        void setAudioData(void* data, int32_t numFrames, int32_t channelsInAudioData);

        void processAudio(AudioData *audioData, int32_t numFrames) override;

        void addMidiEvent(uint8_t *data, int32_t dataLength);

        void playAudioData();

        void startProcessing();

        void pauseProcessing();

        void enableAudioRecorder();
    };

    // Not planned to implement so far.
    class BasicAudioGraph : public AudioGraph {
    public:
        BasicAudioGraph(int32_t framesPerCallback, int32_t channelsInAudioBus) :
                AudioGraph(framesPerCallback, channelsInAudioBus) {
        }

        void attachNode(AudioGraphNode* sourceNode, int32_t sourceOutputBusIndex, AudioGraphNode* destinationNode, int32_t destinationInputBusIndex) {}
        void detachNode(AudioGraphNode* sourceNode, int32_t sourceOutputBusIndex) {}
    };
}

#endif //AAP_CORE_AUDIOGRAPH_H
