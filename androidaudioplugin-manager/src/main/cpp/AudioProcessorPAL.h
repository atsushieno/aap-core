//
// Created by atsushi on 2023/07/18.
//

#ifndef AAP_CORE_AUDIOPROCESSORPAL_H
#define AAP_CORE_AUDIOPROCESSORPAL_H

#include <stdint.h>

namespace aap {
    // try to keep it compatible with Oboe
    enum AudioProcessorPALStatus : int32_t {
        Running = 0,
        Stopped = 1
    };

    class AudioProcessorPAL {
    public:
        virtual bool isRunning() = 0;
        // They are used to control callback-based audio output streaming.
        virtual int32_t setupStream() = 0;
        virtual int32_t startStreaming() = 0;
        virtual int32_t stopStreaming() = 0;
        // It is kind of raw MIDI input event listener (not a "handler" that overrides processing).
        // A stub implementation would override this to dump the messages to Android log.
        virtual void midiInputReceived(uint8_t* bytes, size_t offset, size_t length, int64_t timestampInNanoseconds) {}
    };

}


#endif //AAP_CORE_AUDIOPROCESSORPAL_H
