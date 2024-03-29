#ifndef AAP_CORE_SHARED_MEMORY_EXTENSION_H
#define AAP_CORE_SHARED_MEMORY_EXTENSION_H

#include <sys/mman.h>
#include "plugin-instance.h"

namespace aap {
    class AbstractPluginBuffer
    {
        aap_buffer_t pub;

        static inline int32_t aap_buffer_num_ports(aap_buffer_t& self) {
            return ((AbstractPluginBuffer*) self.impl)->num_ports;
        }
        static inline int32_t aap_buffer_num_frames(aap_buffer_t& self) {
            return ((AbstractPluginBuffer*) self.impl)->num_frames;
        }
        static inline void* aap_buffer_get_buffer(aap_buffer_t& self, int32_t index) {
            return ((AbstractPluginBuffer*) self.impl)->getBuffer(index);
        }
        static inline int32_t aap_buffer_get_buffer_size(aap_buffer_t& self, int32_t index) {
            return ((AbstractPluginBuffer*) self.impl)->getBufferSize(index);
        }

    protected:
        int32_t num_ports{0};
        int32_t num_frames{0};
        void** buffers{nullptr};
        int32_t* buffer_sizes{nullptr};

    public:
        virtual ~AbstractPluginBuffer() {
            if (buffer_sizes)
                free(buffer_sizes);
            if (buffers)
                free(buffers);
        }

        virtual int32_t getPortContentType(int32_t portIndex) = 0;
        virtual int32_t getPortDirection(int32_t portIndex) = 0;

        bool initialize(int32_t numPorts, int32_t numFrames);

        aap_buffer_t* toPublicApi() {
            pub.impl = this;
            pub.num_ports = aap_buffer_num_ports;
            pub.num_frames = aap_buffer_num_frames;
            pub.get_buffer = aap_buffer_get_buffer;
            pub.get_buffer_size = aap_buffer_get_buffer_size;
            return &pub;
        }

        inline int32_t numPorts() { return num_ports; }
        inline int32_t numFrames() { return num_frames; }
        inline void* getBuffer(int32_t bufferIndex) {
            return 0 <= bufferIndex && bufferIndex < num_ports ? buffers[bufferIndex] : nullptr;
        }
        inline int32_t getBufferSize(int32_t bufferIndex) {
            if (0 <= bufferIndex && bufferIndex < num_ports) {
                int32_t size = buffer_sizes[bufferIndex];
                if (size > 0)
                    return size;
                switch (getPortContentType(bufferIndex)) {
                    case AAP_CONTENT_TYPE_AUDIO:
                        return num_frames * sizeof(float);
                    case AAP_CONTENT_TYPE_MIDI2:
                        return DEFAULT_CONTROL_BUFFER_SIZE;
                }
                return 0;
            }
            return 0;
        }
    };

    class SharedMemoryPluginBuffer : public AbstractPluginBuffer {
        PluginInstance* instance;
    public:
        SharedMemoryPluginBuffer(PluginInstance* instance) : instance(instance) {}

        int32_t getPortContentType(int32_t portIndex) override { return instance->getPort(portIndex)->getContentType(); }
        int32_t getPortDirection(int32_t portIndex) override { return instance->getPort(portIndex)->getPortDirection(); }

        inline void setBuffer(size_t index, void* buffer) { buffers[index] = buffer; }

        void unmapSharedMemory() {
            for (size_t i = 0; i < numPorts(); i++) {
                auto buffer = buffers[i];
                if (buffer)
                    munmap(buffer, getBufferSize(i));
            }
        }
    };

    class PluginSharedMemoryStore {
    protected:
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
        std::unique_ptr<SharedMemoryPluginBuffer> port_buffer{nullptr};
        std::map<std::string,int32_t> extension_uri_to_index{};

    public:
        enum PluginMemoryAllocatorResult {
            PLUGIN_MEMORY_ALLOCATOR_SUCCESS,
            PLUGIN_MEMORY_ALLOCATOR_FAILED_LOCAL_ALLOC,
            PLUGIN_MEMORY_ALLOCATOR_FAILED_SHM_CREATE,
            PLUGIN_MEMORY_ALLOCATOR_FAILED_MMAP
        };

        static const char* getMemoryAllocationErrorMessage(int32_t code) {
            switch (code) {
                case PLUGIN_MEMORY_ALLOCATOR_FAILED_LOCAL_ALLOC:
                    return "Plugin client failed at allocating memory.";
                case PLUGIN_MEMORY_ALLOCATOR_FAILED_SHM_CREATE:
                    return "Plugin client failed at creating shm.";
                case PLUGIN_MEMORY_ALLOCATOR_FAILED_MMAP:
                    return "Plugin client failed at mmap.";
                default:
                    return nullptr;
            }
        }

        PluginSharedMemoryStore() {
            // They are all added only via addExtensionFD().
            // Buffers are only mmap()-ed and should always be munmap()-ed at destructor.
            extension_fds = std::make_unique<std::vector<int32_t>>();
            extension_buffers = std::make_unique<std::vector<void*>>();
            extension_buffer_sizes = std::make_unique<std::vector<int32_t>>();
            cached_shm_fds_for_prepare = std::make_unique<std::vector<int32_t>>();

            port_buffer_fds = std::make_unique<std::vector<int32_t>>();
            if (!port_buffer_fds)
                AAP_ASSERT_FALSE;
        }

        virtual ~PluginSharedMemoryStore() {
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
            }
            extension_fds->clear();
        }

        void disposeAudioBufferFDs() {
            // ex-PluginSharedMemoryBuffer part
            if (port_buffer)
                port_buffer->unmapSharedMemory();
            // close the fd. AudioPluginService also dup()-s it, so it has to be closed too.
            for (size_t i = 0; i < port_buffer_fds->size(); i++) {
                auto fd = port_buffer_fds->at(i);
                if (fd >= 0)
                    close(fd);
            }
            port_buffer_fds->clear();
        }

        // Stores clone of port buffer FDs passed from client via Binder.
        inline void resizePortBufferByCount(size_t newSize) {
            cached_shm_fds_for_prepare->resize(newSize);
        }

        // used by AudioPluginInterfaceImpl.
        inline int32_t getPortBufferFD(size_t index) { return port_buffer_fds->at(index); }

        // called by AudioPluginInterfaceImpl::prepareMemory().
        // `fd` is an already-duplicated FD.`
        inline void setPortBufferFD(size_t index, int32_t fd) {
            cached_shm_fds_for_prepare->at(index) = fd;
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

        // So far it is used only by aap_client_as_plugin.
        aap_buffer_t* getAudioPluginBuffer() {
            if (!port_buffer) { // make sure to call allocate*Buffer() first.
                AAP_ASSERT_FALSE;
                return nullptr;
            }
            return port_buffer->toPublicApi();
        }

        size_t getExtensionBufferCount() { return extension_buffer_sizes->size(); }
        std::map<std::string,int32_t>& getExtensionUriToIndexMap() { return extension_uri_to_index; }
        void* getExtensionBuffer(int index) {
            return extension_buffers->at(index);
        }
        size_t getExtensionBufferCapacity(int index) {
            return extension_buffer_sizes->at(index);
        }
    };

    class ClientPluginSharedMemoryStore : public PluginSharedMemoryStore {
    public:
        [[nodiscard]] int32_t allocateClientBuffer(size_t numPorts, size_t numFrames, aap::PluginInstance& instance, size_t defaultControllBytesPerBlock);
    };

    class ServicePluginSharedMemoryStore : public PluginSharedMemoryStore {
    public:
        [[nodiscard]] int32_t allocateServiceBuffer(std::vector<int32_t>& clientFDs, size_t numFrames, aap::PluginInstance& instance, size_t defaultControllBytesPerBlock);

        [[nodiscard]] bool completeServiceInitialization(size_t numFrames, aap::PluginInstance& instance, size_t defaultControllBytesPerBlock) {
            auto ret = allocateServiceBuffer(*cached_shm_fds_for_prepare, numFrames, instance, defaultControllBytesPerBlock) == PluginMemoryAllocatorResult::PLUGIN_MEMORY_ALLOCATOR_SUCCESS;
            cached_shm_fds_for_prepare->clear();
            return ret;
        }
    };
}

#endif //AAP_CORE_SHARED_MEMORY_EXTENSION_H
