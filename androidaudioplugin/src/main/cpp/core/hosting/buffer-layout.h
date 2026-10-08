#ifndef AAP_CORE_BUFFER_LAYOUT_H
#define AAP_CORE_BUFFER_LAYOUT_H

#include <string>
#include "aap/core/aapxs/buses-aapxs.h"

namespace aap { class PluginInstance; }

namespace aap::internal {
    // Every port buffer in the pool starts at this alignment.
    inline constexpr uint32_t BUFFER_LAYOUT_ALIGNMENT = 64;

    // Lays out every port of the instance (in port index order) in one pool. Returns an error, or empty.
    std::string computeBufferLayout(PluginInstance& instance, uint32_t generation, int32_t frameCount,
                                    int32_t controlBytesPerBlock, aap_buffer_layout_t& layout);

    // Checks that a layout sent by the client fits the instance ports and the actual pool size.
    // Returns an error, or empty.
    std::string validateBufferLayout(const aap_buffer_layout_t& layout, PluginInstance& instance,
                                     int32_t frameCount, size_t actualPoolSize);
}

#endif //AAP_CORE_BUFFER_LAYOUT_H
