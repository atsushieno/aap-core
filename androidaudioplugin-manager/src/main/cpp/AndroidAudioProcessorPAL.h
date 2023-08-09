
#ifndef AAP_ANDROID_AUDIO_PROCESSOR_PAL_H
#define AAP_ANDROID_AUDIO_PROCESSOR_PAL_H

#include <thread> // NDK has no jthread...
#include <android/sharedmem.h>
#include <oboe/Oboe.h>
#include "AudioProcessorPAL.h"
#include "AudioProcessor.h"
#include <aap/core/host/android/audio-plugin-host-android.h>
#include <aap/unstable/logging.h>

namespace aap {

    class InterleavedAudioProcessorPAL : public AudioProcessorPAL {
        class Impl;
        Impl* impl;

    public:
        InterleavedAudioProcessorPAL(AudioProcessor *owner);
        virtual ~InterleavedAudioProcessorPAL();
        void processInterleavedAudioIO(void* audioData, int32_t numFrames);
    };

    class AndroidAudioProcessorPAL : public InterleavedAudioProcessorPAL {
    public:
        AndroidAudioProcessorPAL(AudioProcessor *owner) : InterleavedAudioProcessorPAL(owner) {}
        virtual ~AndroidAudioProcessorPAL() {}

        virtual void setBufferCapacityInFrames(int32_t size) = 0;
        virtual void setFramesPerDataCallback(int32_t size) = 0;
    };

    class StubAudioProcessorPAL : public AndroidAudioProcessorPAL {
        bool alive{true};
        void* audioData;

        std::unique_ptr<std::thread> callback_thread{nullptr};
        void runStreamingLoop() {
            while (alive) {
                sleep(1);
                processInterleavedAudioIO(audioData, 1024);
            }
        }

    public:
        StubAudioProcessorPAL(AudioProcessor *owner)
                : AndroidAudioProcessorPAL(owner) {
            audioData = calloc(sizeof(float), 1024 * 2);
        }

        ~StubAudioProcessorPAL() {
            if (audioData)
                free(audioData);
        }

        bool isRunning() override { return alive && callback_thread != nullptr; }
        int32_t setupStream() override { return 0; }
        int32_t startStreaming() override {
            assert(!callback_thread);
            callback_thread = std::make_unique<std::thread>(static_cast<void(StubAudioProcessorPAL::*)()>(&StubAudioProcessorPAL::runStreamingLoop), this);
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

    class OboeAudioProcessorPAL : public oboe::AudioStreamCallback, public AndroidAudioProcessorPAL {
        AudioProcessor *owner;
        oboe::AudioStreamBuilder builder{};
        std::unique_ptr<oboe::StabilizedCallback> callback{};
        std::shared_ptr<oboe::AudioStream> stream_in{};
        std::shared_ptr<oboe::AudioStream> stream_out{};

    public:
        OboeAudioProcessorPAL(AudioProcessor *owner)
                : AndroidAudioProcessorPAL(owner) {
            callback = std::make_unique<oboe::StabilizedCallback>(this);
        }

        oboe::DataCallbackResult
        onAudioReady(oboe::AudioStream *audioStream, void *audioData, int32_t numFrames) override;

        bool isRunning() override;
        int32_t setupStream() override;
        int32_t startStreaming() override;
        int32_t stopStreaming() override;

        inline void setBufferCapacityInFrames(int32_t size) override { builder.setBufferCapacityInFrames(size); }
        inline void setFramesPerDataCallback(int32_t size) override { builder.setFramesPerDataCallback(size); }
    };
}

#endif //AAP_ANDROID_AUDIO_PROCESSOR_PAL_H
