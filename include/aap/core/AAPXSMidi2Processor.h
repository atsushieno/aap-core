#ifndef AAP_CORE_AAPXSMIDI2PROCESSOR_H
#define AAP_CORE_AAPXSMIDI2PROCESSOR_H

#include <functional>
#include "../android-audio-plugin.h"
#include "aap_midi2_helper.h"
#include "aap/aapxs.h"

namespace aap {
    class AAPXSMidi2Processor {
        aap_midi2_aapxs_parse_context aapxs_parse_context{};

        std::function<void(aap_midi2_aapxs_parse_context*)> call_extension;
    public:
        AAPXSMidi2Processor();
        virtual ~AAPXSMidi2Processor();

        uint8_t* midi2_aapxs_data_buffer{nullptr};
        uint8_t* midi2_aapxs_conversion_helper_buffer{nullptr};

        void setExtensionCallback(std::function<void(aap_midi2_aapxs_parse_context*)> caller) {
            call_extension = caller;
        }

        void process(void* buffer);

        void
        addReply(void (*addMidi2Event)(AAPXSMidi2Processor *, void *, int32_t),
                 void *addMidi2EventUserData,
                 int32_t group, int32_t requestId, AAPXSServiceInstance *aapxsInstance, int32_t messageSize,
                 int32_t opcode);
    };
}

#endif //AAP_CORE_AAPXSMIDI2PROCESSOR_H
