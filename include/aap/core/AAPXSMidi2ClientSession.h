#ifndef AAP_CORE_AAPXSMIDI2CLIENTSESSION_H
#define AAP_CORE_AAPXSMIDI2CLIENTSESSION_H

#include <stdint.h>
#include <functional>
#include <map>
#include <future>
#include "aap_midi2_helper.h"
#include "aap/aapxs.h"

namespace aap {
    class AAPXSMidi2ClientSession;
    const size_t MAX_PENDING_CALLBACKS = UINT8_MAX;

    typedef void (*add_midi2_event_func) (AAPXSMidi2ClientSession* session, void* userData, int32_t messageSize);

    class AAPXSMidi2ClientSession {

        struct CallbackUnit {
            uint32_t request_id;
            aapxs_completion_callback func;
            void* data;
        };

        int32_t midi_buffer_size;
        aap_midi2_aapxs_parse_context aapxs_parse_context{};
        std::function<void(aap_midi2_aapxs_parse_context*)> handle_reply;
        CallbackUnit pending_callbacks[MAX_PENDING_CALLBACKS];

    public:
        AAPXSMidi2ClientSession(int32_t midiBufferSize);
        ~AAPXSMidi2ClientSession();

        uint8_t *aapxs_rt_midi_buffer{nullptr};
        uint8_t *aapxs_rt_conversion_helper_buffer{nullptr};

        void setReplyHandler(std::function<void(aap_midi2_aapxs_parse_context*)> handleReply) {
            handle_reply = handleReply;
        }

        void addSession(add_midi2_event_func addMidi2Event,
                        void* addMidi2EventUserData,
                        int32_t group,
                        int32_t requestId,
                        const char* uri,
                        void* data,
                        int32_t dataSize,
                        int32_t opcode);

        void addSession(add_midi2_event_func addMidi2Event, void* addMidi2EventUserData, AAPXSRequestContext* request);

        void completeSession(void* buffer, void* pluginOrHost);
    };
}

#endif //AAP_CORE_AAPXSMIDI2CLIENTSESSION_H
