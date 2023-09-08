#include "aap/core/AAPXSMidi2Processor.h"
#include "aap/ext/midi.h"


aap::AAPXSMidi2Processor::AAPXSMidi2Processor() {
    midi2_aapxs_data_buffer = (uint8_t*) calloc(1, AAP_MIDI2_AAPXS_DATA_MAX_SIZE);
    midi2_aapxs_conversion_helper_buffer = (uint8_t*) calloc(1, AAP_MIDI2_AAPXS_DATA_MAX_SIZE);
    aap_midi2_aapxs_parse_context_prepare(&aapxs_parse_context,
                                          midi2_aapxs_data_buffer,
                                          midi2_aapxs_conversion_helper_buffer,
                                          AAP_MIDI2_AAPXS_DATA_MAX_SIZE);
}

aap::AAPXSMidi2Processor::~AAPXSMidi2Processor() {
    if (midi2_aapxs_data_buffer)
        free(midi2_aapxs_data_buffer);
    if (midi2_aapxs_conversion_helper_buffer)
        free(midi2_aapxs_conversion_helper_buffer);
}

void aap::AAPXSMidi2Processor::process(void* buffer) {
    auto mbh = (AAPMidiBufferHeader *) buffer;
    void* data = mbh + 1;
    CMIDI2_UMP_SEQUENCE_FOREACH(data, mbh->length, iter) {
        auto umpSize = mbh->length - ((uint8_t*) iter - (uint8_t*) data);
        if (aap_midi2_parse_aapxs_sysex8(&aapxs_parse_context, iter, umpSize))
            call_extension(&aapxs_parse_context);

        // FIXME: should we remove those extension bytes from the UMP buffer?
        //  It is going to be extraneous to the plugin.
    }
}

