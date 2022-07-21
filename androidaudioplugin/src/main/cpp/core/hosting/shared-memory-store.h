#ifndef AAP_CORE_SHARED_MEMORY_EXTENSION_H
#define AAP_CORE_SHARED_MEMORY_EXTENSION_H

#include "audio-plugin-host-internals.h"

namespace aap {

class PluginSharedMemoryStore {
    /*
     * Memory allocation and mmap-ing strategy differ between client and service.
     */
    enum PluginBufferOrigin {
        PLUGIN_BUFFER_ORIGIN_UNALLOCATED,
        PLUGIN_BUFFER_ORIGIN_LOCAL,
        PLUGIN_BUFFER_ORIGIN_REMOTE
    };

    PluginBufferOrigin memory_origin{PLUGIN_BUFFER_ORIGIN_UNALLOCATED};

    // They are created by client-as-plugin.
    std::unique_ptr<std::vector<int32_t>> extension_fds{nullptr};
    std::unique_ptr<std::vector<void*>> extension_buffers{nullptr};
    std::unique_ptr<std::vector<int32_t>> extension_buffer_sizes{nullptr};

    // This is a temporary FD store for plugin services.
    // The unknown number of calls of AIDL prepareMemory() precedes prepare(), so we have to
    // first store those FDs somewhere.
    // Within the AIDL, we first receive unknown number of shm FDs.
    std::unique_ptr<std::vector<int32_t>> cached_shm_fds_for_prepare{nullptr};

    // ex-PluginSharedMemoryBuffer members
    std::unique_ptr<std::vector<int32_t>> port_buffer_fds{nullptr};

    // When shms are locally allocated (PLUGIN_BUFFER_ORIGIN_LOCAL), then those buffers are locally calloc()-ed.
    // Otherwise they are just mmap()-ed and should not be freed by own.
    std::unique_ptr<AndroidAudioPluginBuffer> port_buffer{nullptr};

public:
    enum PluginMemoryAllocatorResult {
        PLUGIN_MEMORY_ALLOCATOR_SUCCESS,
        PLUGIN_MEMORY_ALLOCATOR_FAILED_LOCAL_ALLOC,
        PLUGIN_MEMORY_ALLOCATOR_FAILED_SHM_CREATE,
        PLUGIN_MEMORY_ALLOCATOR_FAILED_MMAP
    };

    PluginSharedMemoryStore() {
        // They are all added only via addExtensionFD().
        // Buffers are only mmap()-ed and should always be munmap()-ed at destructor.
        extension_fds = std::make_unique<std::vector<int32_t>>();
        extension_buffers = std::make_unique<std::vector<void*>>();
        extension_buffer_sizes = std::make_unique<std::vector<int32_t>>();
        cached_shm_fds_for_prepare = std::make_unique<std::vector<int32_t>>();

        // ex-PluginSharedMemoryBuffer part
        port_buffer = std::make_unique<AndroidAudioPluginBuffer>();
        assert(port_buffer);
        port_buffer->num_buffers = 0;
        port_buffer->num_frames = 0;
        port_buffer_fds = std::make_unique<std::vector<int32_t>>();
        assert(port_buffer_fds);
    }

    ~PluginSharedMemoryStore() {
        disposeExtensionFDs();
        disposeAudioBufferFDs();
    }

    void disposeExtensionFDs() {
        for (int i = 0; i < extension_fds->size(); i++) {
            if (extension_buffers->at(i))
                munmap(extension_buffers->at(i), (size_t) extension_buffer_sizes->at(i));
            // close the fd. AudioPluginService also dup()-s it, so it has to be closed too.
            auto fd = extension_fds->at(i);
            if (fd >= 0)
                close(fd);
            extension_fds->clear();
        }
    }

    void disposeAudioBufferFDs() {
        // ex-PluginSharedMemoryBuffer part
        if (port_buffer) {
            for (size_t i = 0; i < port_buffer->num_buffers; i++) {
                if (port_buffer->buffers[i])
                    munmap(port_buffer->buffers[i], port_buffer->num_frames * sizeof(float *));
            }
            if (port_buffer->buffers)
                free(port_buffer->buffers);
        }
        // close the fd. AudioPluginService also dup()-s it, so it has to be closed too.
        for (size_t i = 0; i < port_buffer_fds->size(); i++) {
            auto fd = port_buffer_fds->at(i);
            if (fd >= 0)
                close(fd);
        }
        port_buffer_fds->clear();
    }

    // Stores clone of port buffer FDs passed from client via Binder.
    inline void resizePortBuffer(size_t newSize) {
        cached_shm_fds_for_prepare->resize(newSize);
    }

    // used by AudioPluginInterfaceImpl.
    inline int32_t getPortBufferFD(size_t index) { return port_buffer_fds->at(index); }

    // called by AudioPluginInterfaceImpl::prepareMemory().
    // `fd` is an already-duplicated FD.`
    inline void setPortBufferFD(size_t index, int32_t fd) {
        cached_shm_fds_for_prepare->at(index) = fd;
    }

    [[nodiscard]] bool completeServiceInitialization(size_t numFrames) {
        auto ret = allocateServiceBuffer(*cached_shm_fds_for_prepare, numFrames) == PluginMemoryAllocatorResult::PLUGIN_MEMORY_ALLOCATOR_SUCCESS;
        cached_shm_fds_for_prepare->clear();
        return ret;
    }

    void* addExtensionFD(int fd, int dataSize) {
        extension_fds->emplace_back(fd);
        extension_buffer_sizes->emplace_back(dataSize);
        if (fd >= 0 && dataSize > 0)
            extension_buffers->emplace_back(mmap(nullptr, dataSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd,0));
        else
            extension_buffers->emplace_back(nullptr);
        return extension_buffers->at(extension_buffers->size() - 1);
    }

    // ex-PluginSharedMemoryBuffer members

    [[nodiscard]] int32_t allocateClientBuffer(size_t numPorts, size_t numFrames);

    [[nodiscard]] int32_t allocateServiceBuffer(std::vector<int32_t>& clientFDs, size_t numFrames);

    // So far it is used only by aap_client_as_plugin.
    AndroidAudioPluginBuffer* getAudioPluginBuffer() { return port_buffer.get(); }
};

}

#endif //AAP_CORE_SHARED_MEMORY_EXTENSION_H
