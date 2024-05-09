#include "aap/core/AAPXSMidi2RecipientSession.h"
#include "aap/ext/midi.h"
#include "../include_cmidi2.h"


aap::AAPXSMidi2RecipientSession::AAPXSMidi2RecipientSession() {
    midi2_aapxs_data_buffer = (uint8_t*) calloc(1, AAP_MIDI2_AAPXS_DATA_MAX_SIZE);
    midi2_aapxs_conversion_helper_buffer = (uint8_t*) calloc(1, AAP_MIDI2_AAPXS_DATA_MAX_SIZE);
    aap_midi2_aapxs_parse_context_prepare(&aapxs_parse_context,
                                          midi2_aapxs_data_buffer,
                                          midi2_aapxs_conversion_helper_buffer,
                                          AAP_MIDI2_AAPXS_DATA_MAX_SIZE);
}

aap::AAPXSMidi2RecipientSession::~AAPXSMidi2RecipientSession() {
    if (midi2_aapxs_data_buffer)
        free(midi2_aapxs_data_buffer);
    if (midi2_aapxs_conversion_helper_buffer)
        free(midi2_aapxs_conversion_helper_buffer);
}

void aap::AAPXSMidi2RecipientSession::process(void* buffer) {
    auto mbh = (AAPMidiBufferHeader *) buffer;
    void* data = mbh + 1;
    CMIDI2_UMP_SEQUENCE_FOREACH(data, mbh->length, iter) {
        auto umpSize = mbh->length - ((uint8_t*) iter - (uint8_t*) data);
        if (aap_midi2_parse_aapxs_sysex8(&aapxs_parse_context, iter, umpSize)) {
            call_extension(&aapxs_parse_context);

            auto ump = (cmidi2_ump*) iter;
            ump += 4;
            while (cmidi2_ump_get_message_type(ump) == CMIDI2_MESSAGE_TYPE_SYSEX8_MDS &&
                    cmidi2_ump_get_status_code(ump) < CMIDI2_SYSEX_END)
                ump += 4;
            iter = (uint8_t*) ump;
        }

        // FIXME: should we remove those AAPXS SysEx8 bytes from the UMP buffer?
        //  It is going to be extraneous to the rest of the plugin processing.
    }
}


void aap::AAPXSMidi2RecipientSession::addReply(
        void (*addMidi2Event)(AAPXSMidi2RecipientSession * processor, void *userData, int32_t messageSize),
        void* addMidi2EventUserData,
        uint8_t extensionUrid,
        const char* extensionUri,
        int32_t group,
        int32_t requestId,
        void* data,
        int32_t dataSize,
        int32_t opcode) {
    size_t size = aap_midi2_generate_aapxs_sysex8((uint32_t*) midi2_aapxs_data_buffer,
                                                  AAP_MIDI2_AAPXS_DATA_MAX_SIZE / sizeof(int32_t),
                                                  (uint8_t*) midi2_aapxs_conversion_helper_buffer,
                                                  AAP_MIDI2_AAPXS_DATA_MAX_SIZE,
                                                  group,
                                                  requestId,
                                                  extensionUrid,
                                                  extensionUri,
                                                  opcode,
                                                  (uint8_t*) data,
                                                  dataSize);
    addMidi2Event(this, addMidi2EventUserData, size);
}
