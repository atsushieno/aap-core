
#include <sys/mman.h>
#include <cstdlib>
#include <memory>
#include <android/sharedmem.h>
#include <android/log.h>
#include <aidl/org/androidaudioplugin/BpAudioPluginInterface.h>
#include <aidl/org/androidaudioplugin/BnAudioPluginInterface.h>
#include "aap/android-audio-plugin.h"
#include "aap/core/host/android/audio-plugin-host-android.h"
#include "aap/core/host/extension-service.h"
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
    aap::PluginInstantiationState proxy_state{aap::PLUGIN_INSTANTIATION_STATE_INITIAL};
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

void aap_client_as_plugin_prepare(AndroidAudioPlugin *plugin, AndroidAudioPluginBuffer* buffer)
{
	auto ctx = (AAPClientContext*) plugin->plugin_specific;
	if (ctx->proxy_state == aap::PLUGIN_INSTANTIATION_STATE_ERROR)
		return;

    int n = buffer->num_buffers;

    // as it could alter existing PluginSharedMemoryBuffer, it implicitly closes extra shm FDs that are (1)insufficient in size, or (2)not needed anymore.
    ctx->shm_buffer = std::make_unique<aap::PluginSharedMemoryBuffer>();

    ctx->shm_buffer->allocateClientBuffer(buffer->num_buffers, buffer->num_frames);

    // allocate shm FDs, first locally, then send it to the target AAP.
    for (int i = 0; i < n; i++) {
        ::ndk::ScopedFileDescriptor sfd;
        sfd.set(ctx->shm_buffer->getFD(i));
        auto status = ctx->proxy->prepareMemory(ctx->instance_id, i, sfd);
        if (!status.isOk()) {
            aap::a_log_f(AAP_LOG_LEVEL_ERROR, "AAP.proxy", "prepareMemory() failed: %s", /*status.getDescription().c_str()*/"(FIXME: due to Android SDK/NDK issue 219987524 we cannot retrieve failure details here)");
            ctx->proxy_state = aap::PLUGIN_INSTANTIATION_STATE_ERROR;
            break;
        }
    }

    if (ctx->proxy_state != aap::PLUGIN_INSTANTIATION_STATE_ERROR) {
        auto status = ctx->proxy->prepare(ctx->instance_id, buffer->num_frames, n);
        if (!status.isOk()) {
            aap::a_log_f(AAP_LOG_LEVEL_ERROR, "AAP.proxy", "prepare() failed: %s", /*status.getDescription().c_str()*/"(FIXME: due to Android SDK/NDK issue 219987524 we cannot retrieve failure details here)");
            ctx->proxy_state = aap::PLUGIN_INSTANTIATION_STATE_ERROR;
        }
    }

    ctx->previous_buffer = ctx->shm_buffer->getAudioPluginBuffer();

    ctx->proxy_state = aap::PLUGIN_INSTANTIATION_STATE_INACTIVE;
}

void aap_client_as_plugin_activate(AndroidAudioPlugin *plugin)
{
	auto ctx = (AAPClientContext*) plugin->plugin_specific;
	if (ctx->proxy_state == aap::PLUGIN_INSTANTIATION_STATE_ERROR)
		return;

    auto status = ctx->proxy->activate(ctx->instance_id);
    if (!status.isOk()) {
        aap::a_log_f(AAP_LOG_LEVEL_ERROR, "AAP.proxy", "activate() failed: %s", /*status.getDescription().c_str()*/"(FIXME: due to Android SDK/NDK issue 219987524 we cannot retrieve failure details here)");
        ctx->proxy_state = aap::PLUGIN_INSTANTIATION_STATE_ERROR;
    }

    ctx->proxy_state = aap::PLUGIN_INSTANTIATION_STATE_ACTIVE;
}

void aap_client_as_plugin_process(AndroidAudioPlugin *plugin,
	AndroidAudioPluginBuffer* buffer,
	long timeoutInNanoseconds)
{
	auto ctx = (AAPClientContext*) plugin->plugin_specific;
	if (ctx->proxy_state == aap::PLUGIN_INSTANTIATION_STATE_ERROR)
		return;

	// FIXME: copy only input ports
	for (size_t i = 0; i < buffer->num_buffers; i++) {
		if (ctx->previous_buffer->buffers[i] != buffer->buffers[i])
			memcpy(ctx->previous_buffer->buffers[i], buffer->buffers[i],
				   buffer->num_frames * sizeof(float));
	}

	auto status = ctx->proxy->process(ctx->instance_id, timeoutInNanoseconds);
    if (!status.isOk()) {
        aap::a_log_f(AAP_LOG_LEVEL_ERROR, "AAP.proxy", "process() failed: %s", /*status.getDescription().c_str()*/"(FIXME: due to Android SDK/NDK issue 219987524 we cannot retrieve failure details here)");
        ctx->proxy_state = aap::PLUGIN_INSTANTIATION_STATE_ERROR;
    }

	// FIXME: copy only output ports
	for (size_t i = 0; i < buffer->num_buffers; i++) {
		if (ctx->previous_buffer->buffers[i] != buffer->buffers[i])
			memcpy(buffer->buffers[i], ctx->previous_buffer->buffers[i],
				   buffer->num_frames * sizeof(float));
	}
}

void aap_client_as_plugin_deactivate(AndroidAudioPlugin *plugin)
{
	auto ctx = (AAPClientContext*) plugin->plugin_specific;
	if (ctx->proxy_state == aap::PLUGIN_INSTANTIATION_STATE_ERROR)
		return;

	auto status = ctx->proxy->deactivate(ctx->instance_id);
    if (!status.isOk()) {
        aap::a_log_f(AAP_LOG_LEVEL_ERROR, "AAP.proxy", "deactivate() failed: %s", /*status.getDescription().c_str()*/"(FIXME: due to Android SDK/NDK issue 219987524 we cannot retrieve failure details here)");
        ctx->proxy_state = aap::PLUGIN_INSTANTIATION_STATE_ERROR;
    }

    ctx->proxy_state = aap::PLUGIN_INSTANTIATION_STATE_INACTIVE;
}

void aap_client_as_plugin_get_state(AndroidAudioPlugin *plugin, AndroidAudioPluginState* result)
{
	auto ctx = (AAPClientContext*) plugin->plugin_specific;
    if (ctx->proxy_state == aap::PLUGIN_INSTANTIATION_STATE_ERROR)
        return;

    int32_t size;
	auto status = ctx->proxy->getStateSize(ctx->instance_id, &size);
    if (!status.isOk()) {
        aap::a_log_f(AAP_LOG_LEVEL_ERROR, "AAP.proxy", "getStateSize() failed: %s", /*status.getDescription().c_str()*/"(due to Android SDK/NDK issue 219987524 we cannot retrieve failure details here)");
        ctx->proxy_state = aap::PLUGIN_INSTANTIATION_STATE_ERROR;
        return;
    }

	ensureStateBuffer(ctx, size);
	// FIXME: Maybe this should be one call for potential state length mismatch, if ever possible (it isn't so far).
	::ndk::ScopedFileDescriptor fd;
	fd.set(ctx->state_ashmem_fd);
	auto status2 = ctx->proxy->getState(ctx->instance_id, fd);
    if (!status2.isOk()) {
        aap::a_log_f(AAP_LOG_LEVEL_ERROR, "AAP.proxy", "getState() failed: %s", /*status.getDescription().c_str()*/"(FIXME: due to Android SDK/NDK issue 219987524 we cannot retrieve failure details here)");
        ctx->proxy_state = aap::PLUGIN_INSTANTIATION_STATE_ERROR;
    }
    result->data_size = ctx->state.data_size;
    result->raw_data = ctx->state.raw_data;
}

void aap_client_as_plugin_set_state(AndroidAudioPlugin *plugin, AndroidAudioPluginState *input)
{
	auto ctx = (AAPClientContext*) plugin->plugin_specific;
	if (ctx->proxy_state == aap::PLUGIN_INSTANTIATION_STATE_ERROR)
		return;

	// we have to ensure that the pointer is shared memory, so use state buffer inside ctx.
	ensureStateBuffer(ctx, input->data_size);
	memcpy((void*) ctx->state.raw_data, input->raw_data, (size_t) input->data_size);
	::ndk::ScopedFileDescriptor fd;
	fd.set(ctx->state_ashmem_fd);
	auto status = ctx->proxy->setState(ctx->instance_id, fd, input->data_size);
    if (!status.isOk()) {
        aap::a_log_f(AAP_LOG_LEVEL_ERROR, "AAP.proxy", "setState() failed: %s", /*status.getDescription().c_str()*/"(FIXME: due to Android SDK/NDK issue 219987524 we cannot retrieve failure details here)");
        ctx->proxy_state = aap::PLUGIN_INSTANTIATION_STATE_ERROR;
    }
}

void* aap_client_as_plugin_get_extension(AndroidAudioPlugin *plugin, const char *uri)
{
	return nullptr;
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
    if (!status.isOk()) {
        aap::a_log_f(AAP_LOG_LEVEL_ERROR, "AAP.proxy", "beginCreate() failed: %s", /*status.getDescription().c_str()*/"(FIXME: due to Android SDK/NDK issue 219987524 we cannot retrieve failure details here)");
        // It will still return the plugin instance anyways, even though it is not really usable.
        ctx->proxy_state = aap::PLUGIN_INSTANTIATION_STATE_ERROR;
    } else {

        auto instance = (aap::RemotePluginInstance *) host->context;

        // It is a nasty workaround to not expose Binder back to RemotePluginInstance; we set a callable function for them here.
        instance->send_extension_message_impl = [ctx](aap::AAPXSClientInstanceWrapper *aapxs,
                                                      int32_t instanceId, int32_t opcode) {
            if (ctx->proxy_state != aap::PLUGIN_INSTANTIATION_STATE_ERROR) {
                auto stat = ctx->proxy->extension(instanceId, aapxs->asPublicApi()->uri, opcode);
                if (!stat.isOk()) {
                    aap::a_log_f(AAP_LOG_LEVEL_ERROR, "AAP.proxy", "extension() failed: %s", /*stat.getDescription().c_str()*/"(FIXME: due to Android SDK/NDK issue 219987524 we cannot retrieve failure details here)");
                    ctx->proxy_state = aap::PLUGIN_INSTANTIATION_STATE_ERROR;
                }
            }
        };

        // Set up shared memory FDs for plugin extension services.
        // We make use of plugin metadata that should list up required and optional extensions.
        instance->setupAAPXSInstances([&](AAPXSClientInstance *ext) {
            // create asharedmem and add as an extension FD, keep it until it is destroyed.
            auto fd = ASharedMemory_create(ext->uri, ext->data_size);
            ctx->shared_memory_extension->getExtensionFDs()->emplace_back(fd);
            if (ext->data_size > 0)
                ext->data = mmap(nullptr, ext->data_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd,
                                 0);

            if (ctx->proxy_state != aap::PLUGIN_INSTANTIATION_STATE_ERROR) {
                ndk::ScopedFileDescriptor sfd{fd};
                auto stat = ctx->proxy->addExtension(ctx->instance_id, ext->uri, sfd, ext->data_size);
                if (!stat.isOk()) {
                    aap::a_log_f(AAP_LOG_LEVEL_ERROR, "AAP.proxy", "addExtension() failed: %s", /*stat.getDescription().c_str()*/"(FIXME: due to Android SDK/NDK issue 219987524 we cannot retrieve failure details here)");
                    ctx->proxy_state = aap::PLUGIN_INSTANTIATION_STATE_ERROR;
                }
            }
        });

        if (ctx->proxy_state != aap::PLUGIN_INSTANTIATION_STATE_ERROR) {
            status = ctx->proxy->endCreate(ctx->instance_id);
            if (!status.isOk()) {
                aap::a_log_f(AAP_LOG_LEVEL_ERROR, "AAP.proxy", "endCreate() failed: %s", /*status.getDescription().c_str()*/"(FIXME: due to Android SDK/NDK issue 219987524 we cannot retrieve failure details here)");
                ctx->proxy_state = aap::PLUGIN_INSTANTIATION_STATE_ERROR;
            }
            ctx->proxy_state = aap::PLUGIN_INSTANTIATION_STATE_INACTIVE;
        }
    }

	return new AndroidAudioPlugin {
		ctx,
		aap_client_as_plugin_prepare,
		aap_client_as_plugin_activate,
		aap_client_as_plugin_process,
		aap_client_as_plugin_deactivate,
		aap_client_as_plugin_get_state,
		aap_client_as_plugin_set_state,
		aap_client_as_plugin_get_extension
		};
}

void aap_client_as_plugin_delete(
		AndroidAudioPluginFactory *pluginFactory,	// unused
		AndroidAudioPlugin *instance)
{
	auto ctx = (AAPClientContext*) instance->plugin_specific;
    ctx->proxy_state = aap::PLUGIN_INSTANTIATION_STATE_TERMINATED;

	delete ctx;
	delete instance;
}

AndroidAudioPluginFactory *GetAndroidAudioPluginFactoryClientBridge(aap::PluginClient* client) {
	return new AndroidAudioPluginFactory{aap_client_as_plugin_new, aap_client_as_plugin_delete, client};
}
