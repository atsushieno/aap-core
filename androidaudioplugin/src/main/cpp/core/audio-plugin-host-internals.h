#ifndef AAP_CORE_AUDIO_PLUGIN_HOST_INTERNALS_H
#define AAP_CORE_AUDIO_PLUGIN_HOST_INTERNALS_H

#include "aap/unstable/logging.h"

#define AAP_SHARED_MEMORY_EXTENSION_URI "aap-extension:org.androidaudioplugin.SharedMemoryExtension"

extern "C" {

#if ANDROID
AndroidAudioPluginFactory *(GetAndroidAudioPluginFactoryClientBridge)();
#else
AndroidAudioPluginFactory* (GetDesktopAudioPluginFactoryClientBridge)();
#endif

}

namespace aap {

//-----------------------------------

// AndroidAudioPluginBuffer holder that is comfortable in C++ memory model.
class PluginBuffer
{
    std::unique_ptr<AndroidAudioPluginBuffer> buffer{nullptr};

public:
    ~PluginBuffer();

    bool allocateBuffer(size_t numPorts, size_t numFrames);

    AndroidAudioPluginBuffer* getAudioPluginBuffer() { return buffer.get(); }
};

class PluginSharedMemoryBuffer
{
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
                for (size_t i = 0; i < this->shared_memory_fds->size(); i++)
                    close(shared_memory_fds->at(i));
        }
    }

    int32_t allocateClientBuffer(size_t numPorts, size_t numFrames);

    int32_t allocateServiceBuffer(std::vector<int32_t>& clientFDs, size_t numFrames);

    inline int32_t getFD(size_t index) { return shared_memory_fds->at(index); }

    AndroidAudioPluginBuffer* getAudioPluginBuffer() { return buffer.get(); }
};

}
#endif // AAP_CORE_AUDIO_PLUGIN_HOST_INTERNALS_H
