#ifndef AAP_CORE_AUDIO_PLUGIN_HOST_INTERNALS_H
#define AAP_CORE_AUDIO_PLUGIN_HOST_INTERNALS_H

#include "aap/unstable/logging.h"

#if ANDROID
AndroidAudioPluginFactory *GetAndroidAudioPluginFactoryClientBridge(aap::PluginClient *client);
#else
AndroidAudioPluginFactory *GetDesktopAudioPluginFactoryClientBridge(aap::PluginClient *client);
#endif

namespace aap {

//-----------------------------------

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
            return 0 <= bufferIndex && bufferIndex < num_ports ? buffer_sizes[bufferIndex] : 0;
        }
    };

    // AndroidAudioPluginBuffer holder that is comfortable in C++ memory management model.
    class PluginBuffer : public AbstractPluginBuffer {
        void*(*allocate)(size_t size);
        void(*free_memory)(void* ptr);

        static inline void* doCalloc(size_t size) { return calloc(size, 1); }

    public:
        PluginBuffer(void*(*allocateFunc)(size_t size) = doCalloc, void(*freeFunc)(void* ptr) = free)
                : allocate(allocateFunc), free_memory(freeFunc) {}

        ~PluginBuffer();

        bool allocateBuffer(PluginInstance& instance, int32_t defaultControlBytesPerBlock);
    };

//-----------------------------------

}
#endif // AAP_CORE_AUDIO_PLUGIN_HOST_INTERNALS_H
