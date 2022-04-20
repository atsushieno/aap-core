#ifndef AAP_CORE_SHARED_MEMORY_EXTENSION_H
#define AAP_CORE_SHARED_MEMORY_EXTENSION_H

#include "audio-plugin-host-internals.h"

namespace aap {

// FIXME: there may be better AAPXSSharedMemoryStore/PluginSharedMemoryBuffer unification.
class AAPXSSharedMemoryStore {
    std::unique_ptr<std::vector<int32_t>> extension_fds{nullptr};
    std::unique_ptr<std::vector<int32_t>> cached_shm_fds_for_prepare{nullptr};
    std::unique_ptr<PluginSharedMemoryBuffer> port_shm_buffers{nullptr};

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

    void completeInitialization(size_t numFrames) {
        port_shm_buffers->allocateServiceBuffer(*cached_shm_fds_for_prepare, numFrames);
        cached_shm_fds_for_prepare->clear();
    }

    inline std::vector<int32_t> *getExtensionFDs() { return extension_fds.get(); }
};

}

#endif //AAP_CORE_SHARED_MEMORY_EXTENSION_H
