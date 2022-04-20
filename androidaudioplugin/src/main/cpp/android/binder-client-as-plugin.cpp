
#include <sys/mman.h>
#include <cstdlib>
#include <memory>
#include <android/sharedmem.h>
#include <android/log.h>
#include <aidl/org/androidaudioplugin/BpAudioPluginInterface.h>
#include <aidl/org/androidaudioplugin/BnAudioPluginInterface.h>
#include "aap/android-audio-plugin.h"
#include "aap/core/host/android/audio-plugin-host-android.h"
#include "aap/unstable/logging.h"
#include "AudioPluginInterfaceImpl.h"
#include "../core/audio-plugin-host-internals.h"
#include "audio-plugin-host-android-internal.h"
#include "AudioPluginNative_jni.h"

// AAP plugin implementation that performs actual work via AAP binder client.

class AAPClientContext {

public:
	AIBinder* binder{nullptr};
	const char *unique_id{nullptr};
	int32_t instance_id{0};
	ndk::SpAIBinder spAIBinder{nullptr};
	// FIXME: there may be better AAPXSSharedMemoryStore/PluginSharedMemoryBuffer unification.
	std::unique_ptr<aap::AAPXSSharedMemoryStore> shared_memory_extension{nullptr};
	std::unique_ptr<aap::PluginSharedMemoryBuffer> shm_buffer{nullptr};
	std::shared_ptr<aidl::org::androidaudioplugin::IAudioPluginInterface> proxy{nullptr};
	AndroidAudioPluginBuffer *previous_buffer{nullptr};
	AndroidAudioPluginState state{};
	int state_ashmem_fd{0};

    int initialize(int sampleRate, const char *pluginUniqueId)
	{
		unique_id = pluginUniqueId;
    	if(binder == nullptr)
    		return 1; // unexpected
        spAIBinder.set(binder);
		proxy = aidl::org::androidaudioplugin::BpAudioPluginInterface::fromBinder(spAIBinder);
		return 0;
    }

    ~AAPClientContext();
};

void releaseStateBuffer(AAPClientContext *ctx)
{
	if (ctx->state.raw_data)
		munmap((void*) ctx->state.raw_data, (size_t) ctx->state.data_size);
	if (ctx->state_ashmem_fd)
		close(ctx->state_ashmem_fd);
}


AAPClientContext::~AAPClientContext() {
	if (instance_id >= 0) {
		releaseStateBuffer(this);
		proxy->destroy(instance_id);
	}
}

void ensureStateBuffer(AAPClientContext *ctx, int bufferSize)
{
	if (ctx->state.raw_data && ctx->state.data_size >= bufferSize)
		return;
	releaseStateBuffer(ctx);
	ctx->state_ashmem_fd = ASharedMemory_create(ctx->unique_id, bufferSize);
	ctx->state.data_size = bufferSize;
	ctx->state.raw_data = mmap(nullptr, bufferSize, PROT_READ | PROT_WRITE, MAP_SHARED, ctx->state_ashmem_fd, 0);
}

void resetBuffers(AAPClientContext *ctx, AndroidAudioPluginBuffer* buffer)
{
	int n = buffer->num_buffers;

    // as it could alter existing PluginSharedMemoryBuffer, it implicitly closes extra shm FDs that are (1)insufficient in size, or (2)not needed anymore.
	ctx->shm_buffer = std::make_unique<aap::PluginSharedMemoryBuffer>();

    ctx->shm_buffer->allocateClientBuffer(buffer->num_buffers, buffer->num_frames);

    // allocate shm FDs, first locally, then send it to the target AAP.
    for (int i = 0; i < n; i++) {
        ::ndk::ScopedFileDescriptor sfd;
        sfd.set(ctx->shm_buffer->getFD(i));
        auto pmStatus = ctx->proxy->prepareMemory(ctx->instance_id, i, sfd);
        assert(pmStatus.isOk());
    }

	auto status = ctx->proxy->prepare(ctx->instance_id, buffer->num_frames, n);
    assert(status.isOk());

	ctx->previous_buffer = ctx->shm_buffer->getAudioPluginBuffer();
}

void aap_client_as_plugin_prepare(AndroidAudioPlugin *plugin, AndroidAudioPluginBuffer* buffer)
{
	auto ctx = (AAPClientContext*) plugin->plugin_specific;
	resetBuffers(ctx, buffer);
}

void aap_client_as_plugin_activate(AndroidAudioPlugin *plugin)
{
	auto ctx = (AAPClientContext*) plugin->plugin_specific;
    auto status = ctx->proxy->activate(ctx->instance_id);
    assert (status.isOk());
}

void aap_client_as_plugin_process(AndroidAudioPlugin *plugin,
	AndroidAudioPluginBuffer* buffer,
	long timeoutInNanoseconds)
{
	auto ctx = (AAPClientContext*) plugin->plugin_specific;

	// FIXME: copy only input ports
	for (size_t i = 0; i < buffer->num_buffers; i++) {
		if (ctx->previous_buffer->buffers[i] != buffer->buffers[i])
			memcpy(ctx->previous_buffer->buffers[i], buffer->buffers[i],
				   buffer->num_frames * sizeof(float));
	}

	auto status = ctx->proxy->process(ctx->instance_id, timeoutInNanoseconds);

	// FIXME: copy only output ports
	for (size_t i = 0; i < buffer->num_buffers; i++) {
		if (ctx->previous_buffer->buffers[i] != buffer->buffers[i])
			memcpy(buffer->buffers[i], ctx->previous_buffer->buffers[i],
				   buffer->num_frames * sizeof(float));
	}

    assert (status.isOk());
}

void aap_client_as_plugin_deactivate(AndroidAudioPlugin *plugin)
{
	auto ctx = (AAPClientContext*) plugin->plugin_specific;
	auto status = ctx->proxy->deactivate(ctx->instance_id);
    assert (status.isOk());
}

void aap_client_as_plugin_get_state(AndroidAudioPlugin *plugin, AndroidAudioPluginState* result)
{
	auto ctx = (AAPClientContext*) plugin->plugin_specific;
	int size;
	auto status = ctx->proxy->getStateSize(ctx->instance_id, &size);
    assert (status.isOk());
	ensureStateBuffer(ctx, size);
	// FIXME: Maybe this should be one call for potential state length mismatch, if ever possible (it isn't so far).
	::ndk::ScopedFileDescriptor fd;
	fd.set(ctx->state_ashmem_fd);
	auto status2 = ctx->proxy->getState(ctx->instance_id, fd);
    assert (status2.isOk());
    result->data_size = ctx->state.data_size;
    result->raw_data = ctx->state.raw_data;
}

void aap_client_as_plugin_set_state(AndroidAudioPlugin *plugin, AndroidAudioPluginState *input)
{
	auto ctx = (AAPClientContext*) plugin->plugin_specific;
	// we have to ensure that the pointer is shared memory, so use state buffer inside ctx.
	ensureStateBuffer(ctx, input->data_size);
	memcpy((void*) ctx->state.raw_data, input->raw_data, (size_t) input->data_size);
	::ndk::ScopedFileDescriptor fd;
	fd.set(ctx->state_ashmem_fd);
	auto status = ctx->proxy->setState(ctx->instance_id, fd, input->data_size);
    assert (status.isOk());
}

AndroidAudioPlugin* aap_client_as_plugin_new(
	AndroidAudioPluginFactory *pluginFactory,
	const char* pluginUniqueId,
	int aapSampleRate,
	AndroidAudioPluginHost* host
	)
{
	assert(pluginFactory != nullptr);
	assert(pluginUniqueId != nullptr);
	assert(host != nullptr);

    auto client = (aap::PluginClient*) pluginFactory->factory_context;
    auto ctx = new AAPClientContext();
    ctx->binder = (AIBinder*) client->getConnections()->getServiceHandleForConnectedPlugin(pluginUniqueId);

	if(ctx->initialize(aapSampleRate, pluginUniqueId))
		return nullptr;
    ctx->shared_memory_extension = std::make_unique<aap::AAPXSSharedMemoryStore>();

    auto status = ctx->proxy->beginCreate(pluginUniqueId, aapSampleRate, &ctx->instance_id);
    assert (status.isOk());

    auto instance = (aap::RemotePluginInstance*) host->context;
    auto pluginInfo = instance->getPluginInformation();
    std::function<void(const char*, AAPXSClientInstance*)> setupShm = [&](const char* uri, AAPXSClientInstance* ext) {
        // create asharedmem and add as an extension FD, keep it until it is destroyed.
        auto fd = ASharedMemory_create(ext->uri, ext->data_size);
        ctx->shared_memory_extension->getExtensionFDs()->emplace_back(fd);
        void* shm = mmap(nullptr, ext->data_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        memcpy(shm, ext->data, ext->data_size);

        ndk::ScopedFileDescriptor sfd{fd};
        ctx->proxy->addExtension(ctx->instance_id, ext->uri, sfd, ext->data_size);
    };
    for (int i = 0, n = pluginInfo->getNumRequiredExtensions(); i < n; i++) {
        auto uri = pluginInfo->getRequiredExtension(i);
        auto ext = instance->getAAPXSWrapper(uri->c_str())->asPublicApi();
        setupShm(uri->c_str(), ext);
	}

    ctx->proxy->endCreate(ctx->instance_id);

	return new AndroidAudioPlugin {
		ctx,
		aap_client_as_plugin_prepare,
		aap_client_as_plugin_activate,
		aap_client_as_plugin_process,
		aap_client_as_plugin_deactivate,
		aap_client_as_plugin_get_state,
		aap_client_as_plugin_set_state,
		};
}

void aap_client_as_plugin_delete(
		AndroidAudioPluginFactory *pluginFactory,	// unused
		AndroidAudioPlugin *instance)
{
	auto ctx = (AAPClientContext*) instance->plugin_specific;

	delete ctx;
	delete instance;
}

AndroidAudioPluginFactory *GetAndroidAudioPluginFactoryClientBridge(aap::PluginClient* client) {
	return new AndroidAudioPluginFactory{aap_client_as_plugin_new, aap_client_as_plugin_delete, client};
}
