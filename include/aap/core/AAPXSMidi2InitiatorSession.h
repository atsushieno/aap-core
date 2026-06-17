#ifndef AAP_CORE_AAPXSMIDI2INITIATORSESSION_H
#define AAP_CORE_AAPXSMIDI2INITIATORSESSION_H

#include <stdint.h>
#include <functional>
#include <map>
#include <future>
#include <chrono>
#include "aap_midi2_helper.h"
#include "aap/aapxs.h"

#ifndef AAPXS_REQUEST_TIMEOUT_DEFAULT_MS
#define AAPXS_REQUEST_TIMEOUT_DEFAULT_MS 1000
#endif

namespace aap {
    class AAPXSMidi2InitiatorSession;
    const size_t MAX_PENDING_CALLBACKS = UINT8_MAX;

    typedef void (*add_midi2_event_func) (AAPXSMidi2InitiatorSession* session, void* userData, int32_t messageSize);

    class AAPXSMidi2InitiatorSession {

        struct CallbackUnit {
            uint32_t request_id;
            aapxs_completion_callback func;
            void* data;
            // failure delivery + deadline for the request/response timeout (MIDI-CI style).
            aapxs_error_callback error_func;
            std::chrono::steady_clock::time_point deadline;
        };

        int32_t midi_buffer_size;
        int32_t request_timeout_ms{AAPXS_REQUEST_TIMEOUT_DEFAULT_MS};
        aap_midi2_aapxs_parse_context aapxs_parse_context{};
        std::function<void(aap_midi2_aapxs_parse_context*)> handle_reply;
        CallbackUnit pending_callbacks[MAX_PENDING_CALLBACKS];

        // Fires "timeout" for any in-flight request whose deadline has passed. Called from
        // completeSession() (i.e. once per process() cycle).
        void sweepTimeouts(void* pluginOrHost);

    public:
        AAPXSMidi2InitiatorSession(int32_t midiBufferSize);
        ~AAPXSMidi2InitiatorSession();

        void setRequestTimeoutMs(int32_t ms) { request_timeout_ms = ms; }

        uint8_t *aapxs_rt_midi_buffer{nullptr};
        uint8_t *aapxs_rt_conversion_helper_buffer{nullptr};

        void setReplyHandler(std::function<void(aap_midi2_aapxs_parse_context*)> handleReply) {
            handle_reply = handleReply;
        }

        void addSession(add_midi2_event_func addMidi2Event,
                        void* addMidi2EventUserData,
                        int32_t group,
                        int32_t requestId,
                        uint8_t urid,
                        const char* uri,
                        void* data,
                        int32_t dataSize,
                        int32_t opcode);

        void addSession(add_midi2_event_func addMidi2Event, void* addMidi2EventUserData, AAPXSRequestContext* request);

        void completeSession(void* buffer, void* pluginOrHost);
    };
}

#endif //AAP_CORE_AAPXSMIDI2INITIATORSESSION_H
