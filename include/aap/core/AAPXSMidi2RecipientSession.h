#ifndef AAP_CORE_AAPXSMIDI2RECIPIENTSESSION_H
#define AAP_CORE_AAPXSMIDI2RECIPIENTSESSION_H

#include <functional>
#include "../android-audio-plugin.h"
#include "aap_midi2_helper.h"

namespace aap {
    class AAPXSMidi2RecipientSession {
        aap_midi2_aapxs_parse_context aapxs_parse_context{};

        std::function<void(aap_midi2_aapxs_parse_context*)> call_extension;
    public:
        AAPXSMidi2RecipientSession();
        virtual ~AAPXSMidi2RecipientSession();

        uint8_t* midi2_aapxs_data_buffer{nullptr};
        uint8_t* midi2_aapxs_conversion_helper_buffer{nullptr};

        void setExtensionCallback(std::function<void(aap_midi2_aapxs_parse_context*)> caller) {
            call_extension = caller;
        }

        void process(void* buffer);

        void
        addReply(void (*addMidi2Event)(AAPXSMidi2RecipientSession *, void *, int32_t),
                 void* addMidi2EventUserData,
                 uint8_t extensionUrid,
                 const char* extensionUri,
                 int32_t group,
                 int32_t requestId,
                 void* data,
                 int32_t dataSize,
                 int32_t opcode);
    };
}

#endif //AAP_CORE_AAPXSMIDI2RECIPIENTSESSION_H
