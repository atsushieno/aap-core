#include "buffer-layout.h"
#include "aap/core/host/plugin-instance.h"
#include "aap/ext/midi.h"
#include <algorithm>
#include <vector>

namespace {
    uint64_t requiredSize(const aap::PortInformation* port, int32_t frameCount) {
        return port->getContentType() == AAP_CONTENT_TYPE_AUDIO ? (uint64_t) frameCount * sizeof(float)
                                                                : sizeof(AAPMidiBufferHeader);
    }
}

std::string aap::internal::computeBufferLayout(PluginInstance& instance, uint32_t generation, int32_t frameCount,
                                               int32_t controlBytesPerBlock, aap_buffer_layout_t& layout) {
    layout = {};
    auto numPorts = instance.getNumPorts();
    if (numPorts > AAP_MAX_BUFFER_LAYOUT_ENTRIES)
        return "too many ports for a buffer layout";
    if (frameCount <= 0 || controlBytesPerBlock < (int32_t) sizeof(AAPMidiBufferHeader))
        return "invalid buffer sizes";
    layout.generation = generation;
    layout.frame_capacity = (uint32_t) frameCount;
    layout.entry_count = numPorts;
    uint64_t offset = 0;
    for (int32_t i = 0; i < numPorts; i++) {
        auto port = instance.getPort(i);
        uint64_t size = port->getContentType() == AAP_CONTENT_TYPE_AUDIO ? (uint64_t) frameCount * sizeof(float)
                                                                          : (uint64_t) controlBytesPerBlock;
        layout.entries[i] = {(uint32_t) offset, (uint32_t) size};
        offset += (size + BUFFER_LAYOUT_ALIGNMENT - 1) / BUFFER_LAYOUT_ALIGNMENT * BUFFER_LAYOUT_ALIGNMENT;
        if (offset > UINT32_MAX)
            return "buffer layout too large";
    }
    layout.pool_size = (uint32_t) std::max<uint64_t>(offset, BUFFER_LAYOUT_ALIGNMENT);
    return {};
}

std::string aap::internal::validateBufferLayout(const aap_buffer_layout_t& layout, PluginInstance& instance,
                                                int32_t frameCount, size_t actualPoolSize) {
    auto numPorts = instance.getNumPorts();
    if (layout.entry_count != numPorts || layout.entry_count > AAP_MAX_BUFFER_LAYOUT_ENTRIES)
        return "buffer layout does not match the ports";
    if (frameCount <= 0 || layout.frame_capacity < (uint32_t) frameCount)
        return "buffer layout is smaller than the frame count";
    if (layout.pool_size > actualPoolSize)
        return "buffer layout exceeds the shared memory";
    std::vector<std::pair<uint64_t, uint64_t>> ranges{};
    for (int32_t i = 0; i < numPorts; i++) {
        auto& entry = layout.entries[i];
        uint64_t begin = entry.offset, end = (uint64_t) entry.offset + entry.size;
        if (begin % alignof(AAPMidiBufferHeader) != 0 || end > layout.pool_size)
            return "buffer layout entry is out of bounds";
        if (entry.size < requiredSize(instance.getPort(i), frameCount))
            return "buffer layout entry is too small";
        ranges.emplace_back(begin, end);
    }
    std::sort(ranges.begin(), ranges.end());
    for (size_t i = 1; i < ranges.size(); i++)
        if (ranges[i].first < ranges[i - 1].second)
            return "buffer layout entries overlap";
    return {};
}
