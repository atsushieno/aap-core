

#include "aap/core/AAPXSMidi2ClientSession.h"
#include <stdlib.h>
#include "aap/core/aap_midi2_helper.h"
#include "aap/ext/midi.h"

aap::AAPXSMidi2ClientSession::AAPXSMidi2ClientSession(int32_t midiBufferSize)
        : midi_buffer_size(midiBufferSize) {
    aapxs_rt_midi_buffer = (uint8_t*) calloc(1, midi_buffer_size);
    aapxs_rt_conversion_helper_buffer = (uint8_t*) calloc(1, midi_buffer_size);
    aap_midi2_aapxs_parse_context_prepare(&aapxs_parse_context,
                                          aapxs_rt_midi_buffer,
                                          aapxs_rt_conversion_helper_buffer,
                                          AAP_MIDI2_AAPXS_DATA_MAX_SIZE);
}

aap::AAPXSMidi2ClientSession::~AAPXSMidi2ClientSession() {
    if (aapxs_rt_midi_buffer)
        free(aapxs_rt_midi_buffer);
    if (aapxs_rt_conversion_helper_buffer)
        free(aapxs_rt_conversion_helper_buffer);
}

void aap::AAPXSMidi2ClientSession::addSession(
        void (*addMidi2Event)(AAPXSMidi2ClientSession * session, void *userData, int32_t messageSize),
        void* addMidi2EventUserData,
        int32_t group,
        int32_t requestId,
        AAPXSClientInstance *aapxsInstance,
        int32_t messageSize,
        int32_t opcode) {
    size_t size = aap_midi2_generate_aapxs_sysex8((uint32_t*) aapxs_rt_midi_buffer,
                                                  midi_buffer_size / sizeof(int32_t),
                                                  (uint8_t*) aapxs_rt_conversion_helper_buffer,
                                                  midi_buffer_size,
                                                  group,
                                                  requestId,
                                                  aapxsInstance->uri,
                                                  opcode,
                                                  (uint8_t*) aapxsInstance->data,
                                                  messageSize);
    addMidi2Event(this, addMidi2EventUserData, size);
}

void aap::AAPXSMidi2ClientSession::processReply(void* buffer) {
    auto mbh = (AAPMidiBufferHeader *) buffer;
    void* data = mbh + 1;
    CMIDI2_UMP_SEQUENCE_FOREACH(data, mbh->length, iter) {
        auto umpSize = mbh->length - ((uint8_t*) iter - (uint8_t*) data);
        if (aap_midi2_parse_aapxs_sysex8(&aapxs_parse_context, iter, umpSize))
            handle_reply(&aapxs_parse_context);

        // FIXME: should we remove those AAPXS SysEx8 from the UMP buffer?
        //  It is going to be extraneous to the host.
    }

}