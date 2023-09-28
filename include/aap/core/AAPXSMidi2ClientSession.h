#ifndef AAP_CORE_AAPXSMIDI2CLIENTSESSION_H
#define AAP_CORE_AAPXSMIDI2CLIENTSESSION_H

#include <stdint.h>
#include <functional>
#include "aap/aapxs.h"
#include "aap_midi2_helper.h"

namespace aap {
    class AAPXSMidi2ClientSession {
        int32_t midi_buffer_size;
        aap_midi2_aapxs_parse_context aapxs_parse_context{};
        std::function<void(aap_midi2_aapxs_parse_context*)> handle_reply;

    public:
        AAPXSMidi2ClientSession(int32_t midiBufferSize);
        ~AAPXSMidi2ClientSession();

        uint8_t *aapxs_rt_midi_buffer{nullptr};
        uint8_t *aapxs_rt_conversion_helper_buffer{nullptr};

        void setReplyHandler(std::function<void(aap_midi2_aapxs_parse_context*)> handleReply) {
            handle_reply = handleReply;
        }

        void addSession(void (*addMidi2Event)(AAPXSMidi2ClientSession *, void *, int32_t),
                        void* addMidi2EventUserData,
                        int32_t group,
                        int32_t requestId,
                        AAPXSClientInstance *aapxsInstance,
                        int32_t messageSize,
                        int32_t opcode);

        void processReply(void* buffer);
    };
}

#endif //AAP_CORE_AAPXSMIDI2CLIENTSESSION_H
