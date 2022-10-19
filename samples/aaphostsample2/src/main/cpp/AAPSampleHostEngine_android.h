
#ifndef AAP_SAMPLE_HOST_ENGINE_ANDROID_H
#define AAP_SAMPLE_HOST_ENGINE_ANDROID_H

#include <thread> // NDK has no jthread...
#include <android/sharedmem.h>
#include <oboe/Oboe.h>
#include "AAPSampleHostEngine.h"
#include <aap/core/host/android/audio-plugin-host-android.h>
#include <aap/unstable/logging.h>

namespace aap::host_engine {

class AAPHostEngineAndroidPAL : public AAPHostEnginePAL {
public:
    virtual ~AAPHostEngineAndroidPAL() {}

    virtual void setBufferCapacityInFrames(int32_t size) = 0;
    virtual void setFramesPerDataCallback(int32_t size) = 0;
};

class AAPHostEngineAndroidStubPAL : public AAPHostEngineAndroidPAL {
    AAPHostEngineBase *owner;
    bool alive{true};
    void* audioData;

    std::unique_ptr<std::thread> callback_thread{nullptr};
    void runStreamingLoop() {
        while (alive) {
            sleep(1);
            owner->onAudioReady(audioData, 1024);
        }
    }

public:
    AAPHostEngineAndroidStubPAL(AAPHostEngineBase *owner)
        : AAPHostEngineAndroidPAL(), owner(owner) {
        audioData = calloc(sizeof(float), 1024 * 2);
    }

    ~AAPHostEngineAndroidStubPAL() {
        if (audioData)
            free(audioData);
    }

    int32_t setupStream() override { return 0; }
    int32_t startStreaming() override {
        assert(!callback_thread);
        callback_thread = std::make_unique<std::thread>(static_cast<void(AAPHostEngineAndroidStubPAL::*)()>(&AAPHostEngineAndroidStubPAL::runStreamingLoop), this);
        return 0;
    }
    int32_t stopStreaming() override {
        assert(callback_thread);
        alive = false;
        callback_thread->join();
        return 0;
    }

    inline void setBufferCapacityInFrames(int32_t size) override {}
    inline void setFramesPerDataCallback(int32_t size) override {}
    void midiInputReceived(uint8_t* bytes, size_t offset, size_t length, int64_t timestampInNanoseconds) override {
        for (size_t i = offset; i < offset + length; i++)
            aap::aprintf("  %x", bytes[i]);
    }
};

class AAPHostEngineAndroidOboePAL : public oboe::AudioStreamDataCallback, public AAPHostEngineAndroidPAL {
    AAPHostEngineBase *owner;
    oboe::AudioStreamBuilder builder{};
    std::unique_ptr<oboe::AudioStreamDataCallback> callback{};
    std::shared_ptr<oboe::AudioStream> stream{};

public:
    AAPHostEngineAndroidOboePAL(AAPHostEngineBase *owner) : owner(owner) {}

    oboe::DataCallbackResult
    onAudioReady(oboe::AudioStream *audioStream, void *audioData, int32_t numFrames) override;

    int32_t setupStream() override;
    int32_t startStreaming() override;
    int32_t stopStreaming() override;

    inline void setBufferCapacityInFrames(int32_t size) override { builder.setBufferCapacityInFrames(size); }
    inline void setFramesPerDataCallback(int32_t size) override {
        // Oboe: "We encourage leaving this unspecified in most cases."
        if (size > 0)
            builder.setFramesPerDataCallback(size);
    }
};

class AAPHostEngineAndroid : public AAPHostEngineBase {
    std::unique_ptr<AAPHostEngineAndroidPAL> androidPAL;
protected:
    AAPHostEngineAndroidPAL* pal() { return androidPAL.get(); }
public:
    AAPHostEngineAndroid(AudioDriverType audioDriverType = AAP_HOST_ENGINE_AUDIO_DRIVER_TYPE_OBOE)
        : androidPAL(audioDriverType == AAP_HOST_ENGINE_AUDIO_DRIVER_TYPE_STUB ? (std::unique_ptr<AAPHostEngineAndroidPAL>)
            std::make_unique<AAPHostEngineAndroidStubPAL>(this) :
            std::make_unique<AAPHostEngineAndroidOboePAL>(this)) {}

    inline void setBufferCapacityInFrames(int32_t oboeBurstFrameSize, int32_t numFramesPerDataCallback) {
        androidPAL->setBufferCapacityInFrames(oboeBurstFrameSize);
        androidPAL->setFramesPerDataCallback(numFramesPerDataCallback);
    }
};

}

#endif //AAP_SAMPLE_HOST_ENGINE_ANDROID_H

