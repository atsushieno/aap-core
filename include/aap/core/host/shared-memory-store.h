#ifndef AAP_CORE_SHARED_MEMORY_EXTENSION_H
#define AAP_CORE_SHARED_MEMORY_EXTENSION_H

#include <sys/mman.h>
#include <unistd.h>
#include "plugin-instance.h"

namespace aap {
    class AbstractPluginBuffer
    {
        aap_buffer_t pub;

        static inline int32_t aap_buffer_num_ports(aap_buffer_t* self) {
            return ((AbstractPluginBuffer*) self->impl)->num_ports;
        }
        static inline int32_t aap_buffer_num_frames(aap_buffer_t* self) {
            return ((AbstractPluginBuffer*) self->impl)->num_frames;
        }
        static inline void* aap_buffer_get_buffer(aap_buffer_t* self, int32_t index) {
            return ((AbstractPluginBuffer*) self->impl)->getBuffer(index);
        }
        static inline int32_t aap_buffer_get_buffer_size(aap_buffer_t* self, int32_t index) {
            return ((AbstractPluginBuffer*) self->impl)->getBufferSize(index);
        }
        static inline int32_t aap_buffer_get_bus_count(aap_buffer_t* self, aap_bus_kind kind, aap_port_direction direction) {
            return ((AbstractPluginBuffer*) self->impl)->getBusCount(kind, direction);
        }
        static inline int32_t aap_buffer_get_audio_channel_count(aap_buffer_t* self, aap_port_direction direction, int32_t busIndex) {
            return ((AbstractPluginBuffer*) self->impl)->getAudioChannelCount(direction, busIndex);
        }
        static inline float** aap_buffer_get_audio_channels(aap_buffer_t* self, aap_port_direction direction, int32_t busIndex) {
            return ((AbstractPluginBuffer*) self->impl)->getAudioChannels(direction, busIndex);
        }
        static inline void* aap_buffer_get_event_buffer(aap_buffer_t* self, aap_port_direction direction, int32_t busIndex) {
            return ((AbstractPluginBuffer*) self->impl)->getEventBuffer(direction, busIndex);
        }
        static inline int32_t aap_buffer_get_event_buffer_capacity(aap_buffer_t* self, aap_port_direction direction, int32_t busIndex) {
            return ((AbstractPluginBuffer*) self->impl)->getEventBufferCapacity(direction, busIndex);
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

        // bus views (see aap_buffer_t). They must be RT-safe.
        virtual int32_t getBusCount(aap_bus_kind kind, aap_port_direction direction) { return 0; }
        virtual int32_t getAudioChannelCount(aap_port_direction direction, int32_t busIndex) { return 0; }
        virtual float** getAudioChannels(aap_port_direction direction, int32_t busIndex) { return nullptr; }
        virtual void* getEventBuffer(aap_port_direction direction, int32_t busIndex) { return nullptr; }
        virtual int32_t getEventBufferCapacity(aap_port_direction direction, int32_t busIndex) { return 0; }

        bool initialize(int32_t numPorts, int32_t numFrames);

        aap_buffer_t* toPublicApi() {
            pub.impl = this;
            pub.num_ports = aap_buffer_num_ports;
            pub.num_frames = aap_buffer_num_frames;
            pub.get_buffer = aap_buffer_get_buffer;
            pub.get_buffer_size = aap_buffer_get_buffer_size;
            pub.get_bus_count = aap_buffer_get_bus_count;
            pub.get_audio_channel_count = aap_buffer_get_audio_channel_count;
            pub.get_audio_channels = aap_buffer_get_audio_channels;
            pub.get_event_buffer = aap_buffer_get_event_buffer;
            pub.get_event_buffer_capacity = aap_buffer_get_event_buffer_capacity;
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
        // false if the buffers point into a pool that is mapped (and unmapped) by its owner.
        bool owns_mappings{true};

        // Per bus, for each (kind, direction), built by rebuildBusViews().
        struct BusView {
            std::vector<float*> channels{};
            int32_t event_port{-1};
        };
        std::array<std::vector<BusView>, 4> bus_views{};
        static size_t viewListIndex(aap_bus_kind kind, aap_port_direction direction) {
            return (kind == AAP_BUS_KIND_EVENT ? 2 : 0) + (direction == AAP_PORT_DIRECTION_OUTPUT ? 1 : 0);
        }
        BusView* getBusView(aap_bus_kind kind, aap_port_direction direction, int32_t busIndex) {
            auto& list = bus_views[viewListIndex(kind, direction)];
            return 0 <= busIndex && (size_t) busIndex < list.size() ? &list[(size_t) busIndex] : nullptr;
        }

    public:
        SharedMemoryPluginBuffer(PluginInstance* instance) : instance(instance) {}

        int32_t getPortContentType(int32_t portIndex) override { return instance->getPort(portIndex)->getContentType(); }
        int32_t getPortDirection(int32_t portIndex) override { return instance->getPort(portIndex)->getPortDirection(); }

        // Must be called after the buffers are assigned (non-RT).
        void rebuildBusViews() {
            for (auto kind : {AAP_BUS_KIND_AUDIO, AAP_BUS_KIND_EVENT})
                for (auto direction : {AAP_PORT_DIRECTION_INPUT, AAP_PORT_DIRECTION_OUTPUT}) {
                    auto& list = bus_views[viewListIndex(kind, direction)];
                    list.clear();
                    for (int32_t b = 0, n = instance->getNumBuses(kind, direction); b < n; b++) {
                        auto bus = instance->getBus(kind, direction, b);
                        BusView view{};
                        if (kind == AAP_BUS_KIND_AUDIO) {
                            for (int32_t ch = 0, nc = bus->getChannelCount(); ch < nc; ch++)
                                view.channels.emplace_back((float*) getBuffer(bus->getPortIndex(ch)));
                        } else
                            view.event_port = bus->getPortIndex();
                        list.emplace_back(std::move(view));
                    }
                }
        }

        int32_t getBusCount(aap_bus_kind kind, aap_port_direction direction) override {
            return (int32_t) bus_views[viewListIndex(kind, direction)].size();
        }
        int32_t getAudioChannelCount(aap_port_direction direction, int32_t busIndex) override {
            auto view = getBusView(AAP_BUS_KIND_AUDIO, direction, busIndex);
            return view ? (int32_t) view->channels.size() : 0;
        }
        float** getAudioChannels(aap_port_direction direction, int32_t busIndex) override {
            auto view = getBusView(AAP_BUS_KIND_AUDIO, direction, busIndex);
            return view && !view->channels.empty() ? view->channels.data() : nullptr;
        }
        void* getEventBuffer(aap_port_direction direction, int32_t busIndex) override {
            auto view = getBusView(AAP_BUS_KIND_EVENT, direction, busIndex);
            return view ? getBuffer(view->event_port) : nullptr;
        }
        int32_t getEventBufferCapacity(aap_port_direction direction, int32_t busIndex) override {
            auto view = getBusView(AAP_BUS_KIND_EVENT, direction, busIndex);
            return view ? getBufferSize(view->event_port) : 0;
        }

        inline void setBuffer(size_t index, void* buffer) { buffers[index] = buffer; }
        inline void setBufferSize(size_t index, int32_t size) { buffer_sizes[index] = size; }
        inline void setOwnsMappings(bool owns) { owns_mappings = owns; }

        void unmapSharedMemory() {
            if (!owns_mappings)
                return;
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

        // Bus mode: all the port buffers live in one shared memory pool (see aap_buffer_layout_t).
        struct BufferPool {
            int32_t fd{-1};
            void* mapping{nullptr};
            size_t size{0};
            ~BufferPool() {
                if (mapping)
                    munmap(mapping, size);
                if (fd >= 0)
                    close(fd);
            }
        };
        std::unique_ptr<BufferPool> pool{nullptr};
        // The previous pool and its buffer stay mapped until the next replacement, so that a stray
        // process() call that still sees them does not touch unmapped memory.
        std::unique_ptr<BufferPool> retired_pool{nullptr};
        std::unique_ptr<SharedMemoryPluginBuffer> retired_port_buffer{nullptr};

        // Builds port buffers that point into the pool mapping, as the layout describes.
        static std::unique_ptr<SharedMemoryPluginBuffer> createPoolBuffer(const aap_buffer_layout_t& layout,
                                                                          void* mapping, PluginInstance& instance) {
            auto buffer = std::make_unique<SharedMemoryPluginBuffer>(&instance);
            if (!buffer->initialize(layout.entry_count, (int32_t) layout.frame_capacity))
                return nullptr;
            buffer->setOwnsMappings(false);
            for (int32_t i = 0; i < layout.entry_count; i++) {
                buffer->setBuffer(i, (uint8_t*) mapping + layout.entries[i].offset);
                buffer->setBufferSize(i, (int32_t) layout.entries[i].size);
            }
            buffer->rebuildBusViews();
            return buffer;
        }

        void installBufferPool(std::unique_ptr<BufferPool> newPool, std::unique_ptr<SharedMemoryPluginBuffer> buffer) {
            retired_port_buffer.reset();
            retired_pool.reset();
            if (pool) {
                retired_port_buffer = std::move(port_buffer);
                retired_pool = std::move(pool);
            } else
                disposeAudioBufferFDs(); // per-port buffers from a former prepare(), if any
            pool = std::move(newPool);
            port_buffer = std::move(buffer);
        }

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

        bool usesBufferPool() const { return pool != nullptr; }
        int32_t getBufferPoolFD() const { return pool ? pool->fd : -1; }

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
        // Bus mode: allocates a new pool for the layout, replacing the current port buffers.
        [[nodiscard]] int32_t allocateClientBufferPool(const aap_buffer_layout_t& layout, aap::PluginInstance& instance);
    };

    class ServicePluginSharedMemoryStore : public PluginSharedMemoryStore {
    public:
        [[nodiscard]] int32_t allocateServiceBuffer(std::vector<int32_t>& clientFDs, size_t numFrames, aap::PluginInstance& instance, size_t defaultControllBytesPerBlock);

        // Bus mode: maps the pool FD passed by prepareMemory(0, ...) after validating the layout.
        // Returns an error, or empty.
        [[nodiscard]] std::string completeServicePoolInitialization(const aap_buffer_layout_t& layout, int32_t frameCount, aap::PluginInstance& instance);

        [[nodiscard]] bool completeServiceInitialization(size_t numFrames, aap::PluginInstance& instance, size_t defaultControllBytesPerBlock) {
            auto ret = allocateServiceBuffer(*cached_shm_fds_for_prepare, numFrames, instance, defaultControllBytesPerBlock) == PluginMemoryAllocatorResult::PLUGIN_MEMORY_ALLOCATOR_SUCCESS;
            cached_shm_fds_for_prepare->clear();
            return ret;
        }
    };
}

#endif //AAP_CORE_SHARED_MEMORY_EXTENSION_H
