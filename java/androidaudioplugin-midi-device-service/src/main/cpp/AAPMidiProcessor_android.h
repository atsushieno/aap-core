
#ifndef AAP_MIDI_DEVICE_SERVICE_AAPMIDIPROCESSOR_ANDROID_H
#define AAP_MIDI_DEVICE_SERVICE_AAPMIDIPROCESSOR_ANDROID_H

#include <android/sharedmem.h>
#include <oboe/Oboe.h>
#include "AAPMidiProcessor.h"
#include <aap/audio-plugin-host-android.h>
#include <aap/logging.h>

namespace aapmidideviceservice {

class AAPMidiProcessorAndroidPAL : public AAPMidiProcessorPAL {
public:
    virtual ~AAPMidiProcessorAndroidPAL() {}

    virtual void setBufferCapacityInFrames(int32_t size) = 0;
    virtual void setFramesPerDataCallback(int32_t size) = 0;
};

class AAPMidiProcessorAndroidStubPAL : public AAPMidiProcessorAndroidPAL {

public:
    AAPMidiProcessorAndroidStubPAL(AAPMidiProcessor *owner) : AAPMidiProcessorAndroidPAL() {}

    int32_t setupStream() override { return 0; }
    int32_t startStreaming() override { return 0; }
    int32_t stopStreaming() override { return 0; }

    inline void setBufferCapacityInFrames(int32_t size) override {}
    inline void setFramesPerDataCallback(int32_t size) override {}
    void midiInputReceived(uint8_t* bytes, size_t offset, size_t length, int64_t timestampInNanoseconds) override {
        aap::aprintf("AAPMidiProcessorAndroidStubPAL::midiInputReceived(timestampInNanoseconds: %d)", timestampInNanoseconds);
        for (size_t i = offset; i < offset + length; i++)
            aap::aprintf("  %x", bytes[i]);
    }
};

class AAPMidiProcessorOboePAL : public oboe::AudioStreamDataCallback, public AAPMidiProcessorAndroidPAL {
    AAPMidiProcessor *owner;
    oboe::AudioStreamBuilder builder{};
    std::unique_ptr<oboe::AudioStreamDataCallback> callback{};
    std::shared_ptr<oboe::AudioStream> stream{};

public:
    AAPMidiProcessorOboePAL(AAPMidiProcessor *owner) : owner(owner) {}

    oboe::DataCallbackResult
    onAudioReady(oboe::AudioStream *audioStream, void *audioData, int32_t numFrames) override;

    int32_t setupStream() override;
    int32_t startStreaming() override;
    int32_t stopStreaming() override;

    inline void setBufferCapacityInFrames(int32_t size) override { builder.setBufferCapacityInFrames(size); }
    inline void setFramesPerDataCallback(int32_t size) override { builder.setFramesPerDataCallback(size); }
};

class AAPMidiProcessorAndroid : public AAPMidiProcessor {
    std::unique_ptr<AAPMidiProcessorAndroidPAL> androidPAL;
protected:
    AAPMidiProcessorAndroidPAL* pal() { return androidPAL.get(); }
public:
    AAPMidiProcessorAndroid(AudioDriverType audioDriverType = AAP_MIDI_PROCESSOR_AUDIO_DRIVER_TYPE_OBOE)
        : androidPAL(audioDriverType == AAP_MIDI_PROCESSOR_AUDIO_DRIVER_TYPE_STUB ? (std::unique_ptr<AAPMidiProcessorAndroidPAL>)
            std::make_unique<AAPMidiProcessorAndroidStubPAL>(this) :
            std::make_unique<AAPMidiProcessorOboePAL>(this)) {}

    inline void initialize(int32_t sampleRate, int32_t oboeBurstFrameSize, int32_t channelCount, int32_t aapFrameSize) {
        androidPAL->setBufferCapacityInFrames(oboeBurstFrameSize);
        androidPAL->setFramesPerDataCallback(aapFrameSize);
        AAPMidiProcessor::initialize(sampleRate, channelCount, aapFrameSize);
    }

    static void registerPluginService(std::unique_ptr<aap::AudioPluginServiceConnection> service);
};

}

#endif //AAP_MIDI_DEVICE_SERVICE_AAPMIDIPROCESSOR_ANDROID_H
