#ifndef AAP_CORE_AAPXSMIDI2CLIENTSESSION_H
#define AAP_CORE_AAPXSMIDI2CLIENTSESSION_H

#include <stdint.h>
#include <functional>
#include <map>
#include <future>
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

        std::map<int32_t, std::promise<int>> promises{};

        std::optional<std::future<int32_t>> addSession(void (*addMidi2Event)(AAPXSMidi2ClientSession *, void *, int32_t),
                        void* addMidi2EventUserData,
                        int32_t group,
                        int32_t requestId,
                        const char* uri,
                        void* data,
                        int32_t dataSize,
                        int32_t opcode,
                        std::optional<std::promise<int32_t>> promise);

        void processReply(void* buffer);
    };
}

#endif //AAP_CORE_AAPXSMIDI2CLIENTSESSION_H
