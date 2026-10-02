

#include "aap/core/AAPXSMidi2InitiatorSession.h"
#include <cstdlib>
#include "aap/core/aap_midi2_helper.h"
#include "aap/ext/midi.h"
#include "aap/unstable/utility.h"
#include "aap/unstable/logging.h"
#include "../include_cmidi2.h"

#define LOG_TAG "AAP.XS"

namespace {
// Requests are sent from any thread, and replies are handled on the audio thread. It guards the
// encoding buffer and the pending callbacks of every session; callbacks are invoked outside it.
aap::NanoSleepLock session_lock{};
}

aap::AAPXSMidi2InitiatorSession::AAPXSMidi2InitiatorSession(int32_t midiBufferSize)
        : midi_buffer_size(midiBufferSize) {
    // Requests are encoded at the beginning of these buffers, possibly on other threads, while
    // replies are parsed on the audio thread; parsing uses the extra space at the end.
    aapxs_rt_midi_buffer = (uint8_t*) calloc(1, midi_buffer_size + AAP_MIDI2_AAPXS_DATA_MAX_SIZE);
    aapxs_rt_conversion_helper_buffer = (uint8_t*) calloc(1, midi_buffer_size + AAP_MIDI2_AAPXS_DATA_MAX_SIZE);
    aap_midi2_aapxs_parse_context_prepare(&aapxs_parse_context,
                                          aapxs_rt_midi_buffer + midi_buffer_size,
                                          aapxs_rt_conversion_helper_buffer + midi_buffer_size,
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
    const std::lock_guard<NanoSleepLock> guard{session_lock};
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
    // store its callback to the pending callbacks, before its reply could arrive
    if (request->callback) {
        const std::lock_guard<NanoSleepLock> guard{session_lock};
        size_t i = 0;
        auto cbu = CallbackUnit{request->request_id, request->callback,
                                            request->callback_user_data,
                                            request->error_callback,
                                            std::chrono::steady_clock::now() +
                                                std::chrono::milliseconds(request_timeout_ms)};
        for (; i < MAX_PENDING_CALLBACKS; i++) {
            if (!pending_callbacks[i].func) {
                pending_callbacks[i] = cbu;
                break;
            }
        }
        if (i == MAX_PENDING_CALLBACKS) {
            aap::a_log(AAP_LOG_LEVEL_ERROR, LOG_TAG, "AAPXSMidi2InitiatorSession reached max pending callbacks.");
            AAP_ASSERT_FALSE; //
        }
    }
    int32_t group = 0; // will we have to give special semantics on it?
    addSession(addMidi2Event, addMidi2EventUserData,
               group,
               request->request_id,
               request->urid,
               request->uri,
               request->serialization->data,
               request->serialization->data_size,
               request->opcode);
}

void aap::AAPXSMidi2InitiatorSession::sweepTimeouts(void* pluginOrHost) {
    auto now = std::chrono::steady_clock::now();
    CallbackUnit expired[MAX_PENDING_CALLBACKS];
    size_t numExpired = 0;
    {
        const std::lock_guard<NanoSleepLock> guard{session_lock};
        for (size_t i = 0; i < MAX_PENDING_CALLBACKS; i++) {
            auto& unit = pending_callbacks[i];
            if (!unit.func)
                continue;
            if (now < unit.deadline)
                continue;
            expired[numExpired++] = unit;
            memset(&unit, 0, sizeof(CallbackUnit));
        }
    }
    for (size_t i = 0; i < numExpired; i++)
        if (expired[i].error_func)
            expired[i].error_func(expired[i].data, pluginOrHost, "timeout");
}

void aap::AAPXSMidi2InitiatorSession::completeSession(void* buffer, void* pluginOrHost) {
    auto mbh = (AAPMidiBufferHeader *) buffer;
    void* data = mbh + 1;
    CMIDI2_UMP_SEQUENCE_FOREACH(data, mbh->length, iter) {
        auto umpSize = mbh->length - ((uint8_t*) iter - (uint8_t*) data);
        if (aap_midi2_parse_aapxs_sysex8(&aapxs_parse_context, iter, umpSize)) {
            handle_reply(&aapxs_parse_context);

            // look for the corresponding pending callback
            CallbackUnit unit{};
            {
                const std::lock_guard<NanoSleepLock> guard{session_lock};
                for (size_t i = 0; i < MAX_PENDING_CALLBACKS; i++) {
                    if (pending_callbacks[i].func && pending_callbacks[i].request_id == aapxs_parse_context.request_id) {
                        unit = pending_callbacks[i];
                        memset(pending_callbacks + i, 0, sizeof(CallbackUnit));
                        break;
                    }
                }
            }
            if (unit.func)
                unit.func(unit.data, pluginOrHost);
        }

        // FIXME: should we remove those AAPXS SysEx8 from the UMP buffer?
        //  It is going to be extraneous to the host.
    }

    // Detect and fail any requests whose reply never arrived within the timeout window.
    sweepTimeouts(pluginOrHost);
}
