

#include "aap/core/AAPXSMidi2InitiatorSession.h"
#include <stdlib.h>
#include <assert.h>
#include "aap/core/aap_midi2_helper.h"
#include "aap/ext/midi.h"

aap::AAPXSMidi2InitiatorSession::AAPXSMidi2InitiatorSession(int32_t midiBufferSize)
        : midi_buffer_size(midiBufferSize) {
    aapxs_rt_midi_buffer = (uint8_t*) calloc(1, midi_buffer_size);
    aapxs_rt_conversion_helper_buffer = (uint8_t*) calloc(1, midi_buffer_size);
    aap_midi2_aapxs_parse_context_prepare(&aapxs_parse_context,
                                          aapxs_rt_midi_buffer,
                                          aapxs_rt_conversion_helper_buffer,
                                          AAP_MIDI2_AAPXS_DATA_MAX_SIZE);
    memset(pending_callbacks, 0, sizeof(CallbackUnit) * MAX_PENDING_CALLBACKS);
}

aap::AAPXSMidi2InitiatorSession::~AAPXSMidi2InitiatorSession() {
    if (aapxs_rt_midi_buffer)
        free(aapxs_rt_midi_buffer);
    if (aapxs_rt_conversion_helper_buffer)
        free(aapxs_rt_conversion_helper_buffer);
}

void aap::AAPXSMidi2InitiatorSession::addSession(
        add_midi2_event_func addMidi2Event,
        void* addMidi2EventUserData,
        int32_t group,
        int32_t requestId,
        uint8_t urid,
        const char* uri,
        void* data,
        int32_t dataSize,
        int32_t opcode) {
    size_t size = aap_midi2_generate_aapxs_sysex8((uint32_t*) aapxs_rt_midi_buffer,
                                                  midi_buffer_size / sizeof(int32_t),
                                                  (uint8_t*) aapxs_rt_conversion_helper_buffer,
                                                  midi_buffer_size,
                                                  group,
                                                  requestId,
                                                  urid,
                                                  uri,
                                                  opcode,
                                                  (uint8_t*) data,
                                                  dataSize);
    addMidi2Event(this, addMidi2EventUserData, size);
}

void aap::AAPXSMidi2InitiatorSession::addSession(add_midi2_event_func addMidi2Event,
                                                 void* addMidi2EventUserData,
                                                 AAPXSRequestContext *request) {
    int32_t group = 0; // will we have to give special semantics on it?
    addSession(addMidi2Event, addMidi2EventUserData,
               group,
               request->request_id,
               request->urid,
               request->uri,
               request->serialization->data,
               request->serialization->data_size,
               request->opcode);
    // store its callback to the pending callbacks
    if (request->callback) {
        size_t i = 0;
        auto cbu = CallbackUnit{request->request_id, request->callback,
                                            request->callback_user_data};
        for (; i < MAX_PENDING_CALLBACKS; i++) {
            if (pending_callbacks[i].request_id == 0) {
                pending_callbacks[i] = cbu;
                break;
            }
        }
        assert(i != MAX_PENDING_CALLBACKS);
    }
}

void aap::AAPXSMidi2InitiatorSession::completeSession(void* buffer, void* pluginOrHost) {
    auto mbh = (AAPMidiBufferHeader *) buffer;
    void* data = mbh + 1;
    CMIDI2_UMP_SEQUENCE_FOREACH(data, mbh->length, iter) {
        auto umpSize = mbh->length - ((uint8_t*) iter - (uint8_t*) data);
        if (aap_midi2_parse_aapxs_sysex8(&aapxs_parse_context, iter, umpSize)) {
            handle_reply(&aapxs_parse_context);

            // look for the corresponding pending callback
            for (size_t i = 0; i < MAX_PENDING_CALLBACKS; i++) {
                if (pending_callbacks[i].request_id == aapxs_parse_context.request_id) {
                    pending_callbacks[i].func(pending_callbacks[i].data, pluginOrHost);
                    memset(pending_callbacks + i, 0, sizeof(CallbackUnit));
                    break;
                }
            }
        }

        // FIXME: should we remove those AAPXS SysEx8 from the UMP buffer?
        //  It is going to be extraneous to the host.
    }
}
