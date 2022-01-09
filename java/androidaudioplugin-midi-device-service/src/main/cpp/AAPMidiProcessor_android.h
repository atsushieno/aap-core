
#ifndef AAP_MIDI_DEVICE_SERVICE_AAPMIDIPROCESSOR_ANDROID_H
#define AAP_MIDI_DEVICE_SERVICE_AAPMIDIPROCESSOR_ANDROID_H

#include <oboe/Oboe.h>
#include "AAPMidiProcessor.h"
#include <aap/audio-plugin-host-android.h>

namespace aapmidideviceservice {

class AAPMidiProcessorAndroidPAL : public oboe::AudioStreamDataCallback, public AAPMidiProcessorPAL {
    AAPMidiProcessor *owner;
    oboe::AudioStreamBuilder builder{};
    std::unique_ptr<oboe::AudioStreamDataCallback> callback{};
    std::shared_ptr<oboe::AudioStream> stream{};

public:
    AAPMidiProcessorAndroidPAL(AAPMidiProcessor *owner) : owner(owner) {}

    oboe::DataCallbackResult
    onAudioReady(oboe::AudioStream *audioStream, void *audioData, int32_t numFrames) override;

    int32_t createSharedMemory(size_t size) override;

    int32_t setupStream() override;
    int32_t startStreaming() override;
    int32_t stopStreaming() override;

    inline void setBufferCapacityInFrames(int32_t size) { builder.setBufferCapacityInFrames(size); }
    inline void setFramesPerDataCallback(int32_t size) { builder.setFramesPerDataCallback(size); }
};

class AAPMidiProcessorAndroid : public AAPMidiProcessor {
    std::unique_ptr<AAPMidiProcessorAndroidPAL> androidPAL;
protected:
    AAPMidiProcessorAndroidPAL* pal() { return androidPAL.get(); }
public:
    AAPMidiProcessorAndroid()
        : androidPAL(std::make_unique<AAPMidiProcessorAndroidPAL>(this)) {
    }

    inline void initialize(int32_t sampleRate, int32_t oboeBurstFrameSize, int32_t channelCount, int32_t aapFrameSize) {
        androidPAL->setBufferCapacityInFrames(oboeBurstFrameSize);
        androidPAL->setFramesPerDataCallback(aapFrameSize);
        AAPMidiProcessor::initialize(sampleRate, channelCount, aapFrameSize);
    }

    static void registerPluginService(std::unique_ptr<aap::AudioPluginServiceConnection> service);
};

}

#endif //AAP_MIDI_DEVICE_SERVICE_AAPMIDIPROCESSOR_ANDROID_H
