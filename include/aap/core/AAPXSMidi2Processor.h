#ifndef AAP_CORE_AAPXSMIDI2PROCESSOR_H
#define AAP_CORE_AAPXSMIDI2PROCESSOR_H

#include "../android-audio-plugin.h"
#include "aap_midi2_helper.h"

namespace aap {
    class AAPXSMidi2Processor {
        uint8_t* midi2_aapxs_data_buffer{nullptr};
        uint8_t* midi2_aapxs_conversion_helper_buffer{nullptr};
        aap_midi2_aapxs_parse_context aapxs_parse_context{};

        std::function<void(aap_midi2_aapxs_parse_context*)> call_extension;
    public:
        AAPXSMidi2Processor();
        virtual ~AAPXSMidi2Processor();

        void setExtensionCallback(std::function<void(aap_midi2_aapxs_parse_context*)> caller) {
            call_extension = caller;
        }

        void process(void* buffer);
    };
}

#endif //AAP_CORE_AAPXSMIDI2PROCESSOR_H
