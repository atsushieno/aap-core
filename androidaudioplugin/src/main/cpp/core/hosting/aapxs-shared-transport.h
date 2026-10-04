#ifndef AAP_CORE_SHARED_AAPXS_TRANSPORT_H
#define AAP_CORE_SHARED_AAPXS_TRANSPORT_H

#include <atomic>
#include <algorithm>
#include <mutex>
#include <string>
#include <vector>
#include <cstring>
#include "aap/aapxs.h"

namespace aap::internal {
// Optional FDs use addExtension's existing mapping operation, never an unknown
// extension opcode (older service dispatchers cannot safely handle one).
inline constexpr const char* AAPXS_TRANSPORT_URI = "urn:androidaudioplugin:aapxs:transport-v1";
inline constexpr uint32_t AAPXS_TRANSPORT_MAGIC = 0x41585431;
inline constexpr uint32_t AAPXS_TRANSPORT_DIRECTIONAL = 1;
inline constexpr uint32_t AAPXS_TRANSPORT_SYSEX8 = 2;
inline constexpr size_t AAPXS_TRANSPORT_DESCRIPTOR_SIZE = 32;
inline constexpr size_t AAPXS_TRANSPORT_BLOCK_PREFIX = 16;
inline std::string aapxsHostDirectionUri(const char* uri) { return std::string(uri) + "#aapxs-plugin-to-host-v1"; }

// The original URI is advertised with its former logical capacity. An old
// service then maps exactly that size; only an opted-in service maps the prefix.
inline size_t aapxsTransportMappedSize(const void* descriptor, size_t descriptorSize,
                                      size_t declaredSize, size_t fdSize) {
    if (!descriptor || descriptorSize < AAPXS_TRANSPORT_DESCRIPTOR_SIZE ||
        declaredSize > SIZE_MAX - AAPXS_TRANSPORT_BLOCK_PREFIX) return declaredSize;
    uint32_t words[3]; memcpy(words, descriptor, sizeof(words));
    if (words[0] == AAPXS_TRANSPORT_MAGIC && words[1] == 1 &&
        (words[2] & AAPXS_TRANSPORT_DIRECTIONAL) && fdSize >= declaredSize + AAPXS_TRANSPORT_BLOCK_PREFIX)
        return declaredSize + AAPXS_TRANSPORT_BLOCK_PREFIX;
    return declaredSize;
}

class SharedAAPXSTransport {
    struct Block {
        AAPXSSerializationContext* plugin;
        AAPXSSerializationContext* host;
        void* plugin_base;
        void* host_base;
        size_t capacity;
        bool usable;
    };
    std::mutex mutex;
    AAPXSSerializationContext descriptor{};
    std::vector<Block> blocks;
    std::atomic<uint32_t> capabilities{0};
    bool applied{false};
    uint32_t word(size_t offset) const {
        uint32_t result{};
        if (descriptor.data && descriptor.data_capacity >= offset + 4)
            memcpy(&result, static_cast<uint8_t*>(descriptor.data) + offset, 4);
        return result;
    }
    void apply(uint32_t flags) {
        if (flags & AAPXS_TRANSPORT_DIRECTIONAL)
            for (auto& block : blocks) {
                if (!block.capacity) continue;
                block.plugin->data = static_cast<uint8_t*>(block.plugin_base) + AAPXS_TRANSPORT_BLOCK_PREFIX;
                block.host->data = static_cast<uint8_t*>(block.host_base) + AAPXS_TRANSPORT_BLOCK_PREFIX;
            }
        capabilities.store(flags, std::memory_order_release);
        applied = true;
    }
public:
    void setDescriptor(AAPXSSerializationContext value, bool client) {
        descriptor = value;
        if (client && value.data && value.data_capacity >= AAPXS_TRANSPORT_DESCRIPTOR_SIZE) {
            uint32_t words[8]{AAPXS_TRANSPORT_MAGIC, 1,
                    AAPXS_TRANSPORT_DIRECTIONAL | AAPXS_TRANSPORT_SYSEX8, 0};
            memcpy(value.data, words, sizeof(words));
        }
    }
    void add(AAPXSSerializationContext* plugin, AAPXSSerializationContext* host, size_t capacity,
             void* pluginBase, size_t pluginMappedSize, void* hostBase, size_t hostMappedSize) {
        auto usable = capacity == 0 || (pluginBase && hostBase &&
                pluginMappedSize >= capacity + AAPXS_TRANSPORT_BLOCK_PREFIX &&
                hostMappedSize >= capacity + AAPXS_TRANSPORT_BLOCK_PREFIX);
        blocks.push_back({plugin, host, pluginBase, hostBase, capacity, usable});
        plugin->data = pluginBase;
        plugin->data_capacity = std::min(capacity, pluginMappedSize);
        // Initially use the legacy shared payload. Only an acknowledged peer switches views.
        host->data = pluginBase;
        host->data_capacity = plugin->data_capacity;
    }
    void acceptService() {
        if (!descriptor.data || descriptor.data_capacity < AAPXS_TRANSPORT_DESCRIPTOR_SIZE ||
            word(0) != AAPXS_TRANSPORT_MAGIC || word(4) != 1) return;
        uint32_t flags = word(8) & (AAPXS_TRANSPORT_DIRECTIONAL | AAPXS_TRANSPORT_SYSEX8);
        for (auto& block : blocks) if (!block.usable) flags &= ~AAPXS_TRANSPORT_DIRECTIONAL;
        apply(flags);
        // Published before plugin construction can invoke a host extension.
        __atomic_store_n(reinterpret_cast<uint32_t*>(static_cast<uint8_t*>(descriptor.data) + 12),
                         flags, __ATOMIC_RELEASE);
    }
    void refreshClient() {
        std::lock_guard<std::mutex> lock(mutex);
        if (applied || !descriptor.data || descriptor.data_capacity < AAPXS_TRANSPORT_DESCRIPTOR_SIZE ||
            word(0) != AAPXS_TRANSPORT_MAGIC || word(4) != 1) return;
        auto flags = __atomic_load_n(reinterpret_cast<uint32_t*>(static_cast<uint8_t*>(descriptor.data) + 12), __ATOMIC_ACQUIRE);
        flags &= word(8) & (AAPXS_TRANSPORT_DIRECTIONAL | AAPXS_TRANSPORT_SYSEX8);
        for (auto& block : blocks) if (!block.usable) flags &= ~AAPXS_TRANSPORT_DIRECTIONAL;
        // An old peer leaves the descriptor untouched. Keep its legacy views.
        if (flags) apply(flags);
    }
    uint32_t getCapabilities() const { return capabilities.load(std::memory_order_acquire); }
};
}
#endif
