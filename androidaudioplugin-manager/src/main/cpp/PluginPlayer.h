#ifndef AAP_CORE_PLUGINPLAYER_H
#define AAP_CORE_PLUGINPLAYER_H

#include "aap/core/host/plugin-instance.h"

namespace aap {

class PluginPlayer {
    aap::PluginListSnapshot pluginServices;
    std::unique_ptr<PluginClientConnectionList> connections;
    std::unique_ptr<PluginClient> client;
    RemotePluginInstance* instance{nullptr};
    size_t ump_buffer_size;
    void* ump_in_buffer{nullptr};
    void* ump_out_buffer{nullptr};

public:
    PluginPlayer() {
        pluginServices = PluginListSnapshot::queryServices();
        connections = std::make_unique<PluginClientConnectionList>();
        client = std::make_unique<PluginClient>(connections.get(), &pluginServices);
    }

    ~PluginPlayer() {
        if (ump_in_buffer)
            free(ump_in_buffer);
        if (ump_out_buffer)
            free(ump_out_buffer);
    }

    void configurePortBuffers(size_t audioPortCount, size_t umpBufferSize) {
        assert(umpBufferSize > 0);
        ump_buffer_size = umpBufferSize;
        ump_in_buffer = calloc(ump_buffer_size, 1);
        ump_out_buffer = calloc(ump_buffer_size, 1);
    }

    void start();
    void pause();

    void enqueueUmp32(uint32_t i1);
    void enqueueUmp64(uint32_t i1, uint32_t i2);
    void enqueueUmp128(uint32_t i1, uint32_t i2, uint32_t i3, uint32_t i4);
    void enqueueUmp(uint32_t* buffer, size_t size);
};

}
#endif //AAP_CORE_PLUGINPLAYER_H
