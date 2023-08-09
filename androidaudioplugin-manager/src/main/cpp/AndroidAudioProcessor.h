//
// Created by atsushi on 2023/07/18.
//

#ifndef AAP_CORE_ANDROIDAUDIOPROCESSOR_H
#define AAP_CORE_ANDROIDAUDIOPROCESSOR_H

#include "AudioProcessor.h"
#include "AndroidAudioProcessorPAL.h"

namespace aap {

    enum AudioDriverType {
        AUDIO_DRIVER_TYPE_STUB,
        AUDIO_DRIVER_TYPE_OBOE
    };

    class AndroidAudioProcessor : public AudioProcessor {
        std::unique_ptr<AndroidAudioProcessorPAL> androidPAL;
    protected:
        AndroidAudioProcessorPAL* pal() { return androidPAL.get(); }
    public:
        AndroidAudioProcessor(AudioDriverType audioDriverType = AUDIO_DRIVER_TYPE_OBOE)
        : androidPAL(audioDriverType == AUDIO_DRIVER_TYPE_STUB ? (std::unique_ptr<AndroidAudioProcessorPAL>)
        std::make_unique<StubAudioProcessorPAL>(this) :
        std::make_unique<OboeAudioProcessorPAL>(this)) {}

        inline void initialize(int32_t sampleRate, int32_t oboeBurstFrameSize, int32_t audioInChannelCount, int32_t audioOutChannelCount, int32_t aapFrameSize, int midiBufferSize) {
            androidPAL->setBufferCapacityInFrames(oboeBurstFrameSize);
            AudioProcessor::initialize(sampleRate, audioInChannelCount, audioOutChannelCount, aapFrameSize, midiBufferSize);
        }
    };

}


#endif //AAP_CORE_ANDROIDAUDIOPROCESSOR_H
