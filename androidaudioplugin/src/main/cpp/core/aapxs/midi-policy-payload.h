#pragma once

#include <algorithm>
#include <cstring>
#include <string>
#include "aap/aapxs.h"

namespace aap::internal {
// Keep the existing length + UTF-8 ID record for compatibility with older services.
inline size_t writeMidiPolicyPluginId(void* data, size_t capacity, const std::string& pluginId) {
    if (!data || pluginId.size() >= AAP_MAX_PLUGIN_ID_SIZE || capacity < sizeof(int32_t) + pluginId.size())
        return 0;
    auto length = static_cast<int32_t>(pluginId.size());
    memcpy(data, &length, sizeof(length));
    memcpy(static_cast<char*>(data) + sizeof(length), pluginId.data(), pluginId.size());
    return sizeof(length) + pluginId.size();
}

inline std::string readMidiPolicyPluginId(const AAPXSSerializationContext& data,
                                          const std::string& instancePluginId) {
    // Instance metadata also supports old clients which sent no ID and left stale shared bytes.
    if (!instancePluginId.empty())
        return instancePluginId;
    if (!data.data || data.data_capacity < sizeof(int32_t))
        return {};
    // Binder has no payload-length field. Zero means unknown there; nonzero lengths can be checked.
    auto size = data.data_size ? std::min(data.data_size, data.data_capacity) : data.data_capacity;
    if (size < sizeof(int32_t))
        return {};
    int32_t length{};
    memcpy(&length, data.data, sizeof(length));
    if (length <= 0 || length >= AAP_MAX_PLUGIN_ID_SIZE || static_cast<size_t>(length) > size - sizeof(length))
        return {};
    return {static_cast<const char*>(data.data) + sizeof(length), static_cast<size_t>(length)};
}
}
