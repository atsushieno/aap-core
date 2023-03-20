#ifndef AAP_CORE_UTILITY_H
#define AAP_CORE_UTILITY_H

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-variable"
#pragma clang diagnostic ignored "-Wunused-function"
#pragma clang diagnostic ignored "-Wunused-but-set-variable"
#include "../../../../../../external/cmidi2/cmidi2.h"
#pragma clang diagnostic pop


namespace aap {
    static inline void merge_event_inputs(void *mergeTmp, void* eventInputs, int32_t eventInputsSize, aap_buffer_t *buffer, PluginInstance* instance) {
        if (eventInputsSize == 0)
            return;
        for (int i = 0; i < instance->getNumPorts(); i++) {
            auto port = instance->getPort(i);
            if (port->getContentType() == AAP_CONTENT_TYPE_MIDI2 && port->getPortDirection() == AAP_PORT_DIRECTION_INPUT) {
                auto mbh = (AAPMidiBufferHeader*) buffer->get_buffer(*buffer, i);
                size_t newSize = cmidi2_ump_merge_sequences((cmidi2_ump*) mergeTmp, eventInputsSize,
                                                            (cmidi2_ump*) eventInputs, (size_t) eventInputsSize,
                                                            (cmidi2_ump*) mbh + 1, (size_t) mbh->length);
                mbh->length = newSize;
                memcpy(mbh + 1, mergeTmp, newSize);
                return;
            }
        }
    }
}

#endif //AAP_CORE_UTILITY_H
