#ifndef AAP_CORE_HOSTING_MIDI2_PORT_BUFFER_H
#define AAP_CORE_HOSTING_MIDI2_PORT_BUFFER_H

#include "aap/android-audio-plugin.h"
#include "aap/ext/midi.h"
#include "../include_cmidi2.h"

namespace aap::internal {

// Returns the MIDI2 buffer of `portIndex`, whose length is written by the other process: it is
// trimmed to the last complete UMP within the buffer, so that it can be iterated safely. RT-safe.
inline AAPMidiBufferHeader* getMidi2PortBuffer(aap_buffer_t* buffer, int32_t portIndex) {
    auto mbh = (AAPMidiBufferHeader*) buffer->get_buffer(buffer, portIndex);
    if (!mbh)
        return nullptr;
    auto size = buffer->get_buffer_size(buffer, portIndex);
    uint32_t capacity = size > (int32_t) sizeof(AAPMidiBufferHeader) ? size - sizeof(AAPMidiBufferHeader) : 0;
    uint32_t length = mbh->length < capacity ? mbh->length : capacity;
    auto data = (uint8_t*) (mbh + 1);
    uint32_t valid = 0;
    while (valid + sizeof(uint32_t) <= length) {
        auto umpSize = cmidi2_ump_get_num_bytes(cmidi2_ump_read_uint32_bytes(data + valid));
        if (valid + umpSize > length)
            break;
        valid += umpSize;
    }
    mbh->length = valid;
    return mbh;
}

}

#endif //AAP_CORE_HOSTING_MIDI2_PORT_BUFFER_H
