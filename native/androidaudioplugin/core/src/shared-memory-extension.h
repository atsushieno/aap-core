#ifndef AAP_CORE_SHARED_MEMORY_EXTENSION_H
#define AAP_CORE_SHARED_MEMORY_EXTENSION_H

namespace aap {

class SharedMemoryExtension {
    std::unique_ptr<std::vector<int32_t>> port_buffer_fds{nullptr};
    std::unique_ptr<std::vector<int32_t>> extension_fds{nullptr};

public:
    SharedMemoryExtension() {
        port_buffer_fds = std::make_unique<std::vector<int32_t>>();
        extension_fds = std::make_unique<std::vector<int32_t>>();
    }

    ~SharedMemoryExtension() {
        for (int32_t fd: *port_buffer_fds)
            if (fd)
                close(fd);
        port_buffer_fds->clear();
        for (int32_t fd: *extension_fds)
            if (fd)
                close(fd);
        extension_fds->clear();
    }

    // Stores clone of port buffer FDs passed from client via Binder.
    inline void resizePortBuffer(size_t newSize) { port_buffer_fds->resize(newSize); }

    inline int32_t getPortBufferFD(size_t index) { return port_buffer_fds->at(index); }

    inline void setPortBufferFD(size_t index, int32_t fd) {
        if (port_buffer_fds->size() <= index)
            port_buffer_fds->resize(index + 1);
        port_buffer_fds->at(index) = fd;
    }

    // Stores clone of extension data FDs passed from client via Binder.
    inline std::vector<int32_t> *getExtensionFDs() { return extension_fds.get(); }
};

inline SharedMemoryExtension *getSharedMemoryExtension(PluginInstance *instance) {
    return (SharedMemoryExtension *) instance->getExtension(AAP_SHARED_MEMORY_EXTENSION_URI);
}

}

#endif //AAP_CORE_SHARED_MEMORY_EXTENSION_H
