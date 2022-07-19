#ifndef AAP_CORE_SHARED_MEMORY_EXTENSION_H
#define AAP_CORE_SHARED_MEMORY_EXTENSION_H

#include "audio-plugin-host-internals.h"

namespace aap {

class PluginSharedMemoryBuffer {
    /*
     * Memory allocation and mmap-ing strategy differ between client and service.
     */
    enum PluginBufferOrigin {
        PLUGIN_BUFFER_ORIGIN_UNALLOCATED,
        PLUGIN_BUFFER_ORIGIN_LOCAL,
        PLUGIN_BUFFER_ORIGIN_REMOTE
    };

    std::unique_ptr<std::vector<int32_t>> shared_memory_fds{nullptr};
    std::unique_ptr<AndroidAudioPluginBuffer> buffer{nullptr};
    PluginBufferOrigin memory_origin{PLUGIN_BUFFER_ORIGIN_UNALLOCATED};

public:
    enum PluginMemoryAllocatorResult {
        PLUGIN_MEMORY_ALLOCATOR_SUCCESS,
        PLUGIN_MEMORY_ALLOCATOR_FAILED_LOCAL_ALLOC,
        PLUGIN_MEMORY_ALLOCATOR_FAILED_SHM_CREATE,
        PLUGIN_MEMORY_ALLOCATOR_FAILED_MMAP
    };

    PluginSharedMemoryBuffer() {
        buffer = std::make_unique<AndroidAudioPluginBuffer>();
        assert(buffer);
        buffer->num_buffers = 0;
        buffer->num_frames = 0;
        shared_memory_fds = std::make_unique<std::vector<int32_t>>();
        assert(shared_memory_fds);
    }

    ~PluginSharedMemoryBuffer() {
        if (buffer) {
            for (size_t i = 0; i < buffer->num_buffers; i++) {
                if (buffer->buffers[i])
                    munmap(buffer->buffers[i], buffer->num_frames * sizeof(float *));
            }
            if (buffer->buffers)
                free(buffer->buffers);
        }
        if (memory_origin == PLUGIN_BUFFER_ORIGIN_LOCAL) {
            for (size_t i = 0; i < shared_memory_fds->size(); i++)
                close(shared_memory_fds->at(i));
            shared_memory_fds->clear();
        }
    }

    [[nodiscard]] int32_t allocateClientBuffer(size_t numPorts, size_t numFrames);

    [[nodiscard]] int32_t allocateServiceBuffer(std::vector<int32_t>& clientFDs, size_t numFrames);

    inline int32_t getFD(size_t index) { return shared_memory_fds->at(index); }

    AndroidAudioPluginBuffer* getAudioPluginBuffer() { return buffer.get(); }
};

// FIXME: there may be better AAPXSSharedMemoryStore/PluginSharedMemoryBuffer unification.
class AAPXSSharedMemoryStore {
    std::unique_ptr<PluginSharedMemoryBuffer> port_shm_buffers{nullptr};
    std::unique_ptr<std::vector<int32_t>> extension_fds{nullptr};
    std::unique_ptr<std::vector<int32_t>> cached_shm_fds_for_prepare{nullptr};

public:
    AAPXSSharedMemoryStore() {
        port_shm_buffers = std::make_unique<PluginSharedMemoryBuffer>();
        extension_fds = std::make_unique<std::vector<int32_t>>();
        cached_shm_fds_for_prepare = std::make_unique<std::vector<int32_t>>();
    }

    ~AAPXSSharedMemoryStore() {
        for (int32_t fd: *extension_fds)
            if (fd)
                close(fd);
        extension_fds->clear();
    }

    inline aap::PluginSharedMemoryBuffer* getShmBuffer() { return port_shm_buffers.get(); }

    // Stores clone of port buffer FDs passed from client via Binder.
    inline void resizePortBuffer(size_t newSize) {
        cached_shm_fds_for_prepare->resize(newSize);
    }

    inline int32_t getPortBufferFD(size_t index) {
        return port_shm_buffers->getFD(index);
    }

    inline void setPortBufferFD(size_t index, int32_t fd) {
        cached_shm_fds_for_prepare->at(index) = fd;
    }

    [[nodiscard]] bool completeServiceInitialization(size_t numFrames) {
        auto ret = port_shm_buffers->allocateServiceBuffer(*cached_shm_fds_for_prepare, numFrames) == PluginSharedMemoryBuffer::PluginMemoryAllocatorResult::PLUGIN_MEMORY_ALLOCATOR_SUCCESS;
        cached_shm_fds_for_prepare->clear();
        return ret;
    }

    inline std::vector<int32_t> *getExtensionFDs() { return extension_fds.get(); }
};

}

#endif //AAP_CORE_SHARED_MEMORY_EXTENSION_H
