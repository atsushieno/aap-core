
#include <sys/mman.h>
#include <cstdlib>
#include <memory>
#include <android/sharedmem.h>
#include <android/log.h>
#include <aidl/org/androidaudioplugin/BpAudioPluginInterface.h>
#include <aidl/org/androidaudioplugin/BnAudioPluginInterface.h>
#include "aap/android-audio-plugin.h"
#include "aap/core/host/android/audio-plugin-host-android.h"
#include "aap/core/aapxs/extension-service.h"
#include "aap/unstable/logging.h"
#include "AudioPluginInterfaceImpl.h"
#include "../core/hosting/audio-plugin-host-internals.h"
#include "audio-plugin-host-android-internal.h"
#include "JNIClientAAPXSManager.h"

#define AAP_PXORY_LOG_TAG "AAP.proxy"

// AAP plugin implementation that performs actual work via AAP binder client.

class AAPClientContext {

public:
	const char *unique_id{nullptr};
	int32_t instance_id{-1};
	std::unique_ptr<aap::PluginSharedMemoryStore> shm_store{nullptr};
	aap::AndroidPluginClientConnectionData* connection_data{nullptr};
	aap_state_extension_t state_ext;
	aap_state_t state{};
    aap::PluginInstantiationState proxy_state{aap::PLUGIN_INSTANTIATION_STATE_INITIAL};
	int state_ashmem_fd{0};

    ~AAPClientContext();

	aidl::org::androidaudioplugin::IAudioPluginInterface* getProxy() { return connection_data->getProxy(); }
};

void releaseStateBuffer(AAPClientContext *ctx)
{
	if (ctx->state.data)
		munmap((void*) ctx->state.data, (size_t) ctx->state.data_size);
	if (ctx->state_ashmem_fd)
		close(ctx->state_ashmem_fd);
}


AAPClientContext::~AAPClientContext() {
	if (instance_id >= 0) {
		releaseStateBuffer(this);
		getProxy()->destroy(instance_id);
        instance_id = -1;
	}
}

const char* getMemoryAllocationErrorMessage(int32_t code) {
	switch (code) {
	case aap::PluginSharedMemoryStore::PluginMemoryAllocatorResult::PLUGIN_MEMORY_ALLOCATOR_FAILED_LOCAL_ALLOC:
		return "Plugin client failed at allocating memory.";
	case aap::PluginSharedMemoryStore::PluginMemoryAllocatorResult::PLUGIN_MEMORY_ALLOCATOR_FAILED_SHM_CREATE:
		return "Plugin client failed at creating shm.";
	case aap::PluginSharedMemoryStore::PluginMemoryAllocatorResult::PLUGIN_MEMORY_ALLOCATOR_FAILED_MMAP:
		return "Plugin client failed at mmap.";
	default:
		return nullptr;
	}
}

void _log_error_with_details(const char* fmtBase, ndk::ScopedAStatus& status) {
#ifdef __ANDROID_UNAVAILABLE_SYMBOLS_ARE_WEAK__
	if (__builtin_available(android 30, *)) {
#else
		if (__ANDROID_API__ >= 30) {
#endif
		aap::a_log_f(AAP_LOG_LEVEL_ERROR, AAP_PXORY_LOG_TAG, (std::string{fmtBase} + ": %s").c_str(),
					 status.getDescription().c_str());
	} else {
		aap::a_log_f(AAP_LOG_LEVEL_ERROR, AAP_PXORY_LOG_TAG, fmtBase);
	}
}

void aap_client_as_plugin_prepare(AndroidAudioPlugin *plugin, AndroidAudioPluginBuffer* buffer)
{
	auto ctx = (AAPClientContext*) plugin->plugin_specific;
	if (ctx->proxy_state == aap::PLUGIN_INSTANTIATION_STATE_ERROR)
		return;

    int n = buffer->num_buffers;

	if (ctx->shm_store->getAudioPluginBuffer()->num_buffers > 0)
		ctx->shm_store->disposeAudioBufferFDs();

    auto code = ctx->shm_store->allocateClientBuffer(buffer->num_buffers, buffer->num_frames);
	if (code != aap::PluginSharedMemoryStore::PluginMemoryAllocatorResult::PLUGIN_MEMORY_ALLOCATOR_SUCCESS) {
		aap::a_log(AAP_LOG_LEVEL_ERROR, "AAP", getMemoryAllocationErrorMessage(code));
		ctx->proxy_state = aap::PLUGIN_INSTANTIATION_STATE_ERROR;
		return;
	}

	if (ctx->proxy_state != aap::PLUGIN_INSTANTIATION_STATE_ERROR) {
		auto status = ctx->getProxy()->beginPrepare(ctx->instance_id);
		if (!status.isOk()) {
			_log_error_with_details("beginPrepare() failed", status);
			ctx->proxy_state = aap::PLUGIN_INSTANTIATION_STATE_ERROR;
		}
	}

    // allocate shm FDs, first locally, then send it to the target AAP.
    for (int i = 0; i < n; i++) {
		auto fd = ctx->shm_store->getPortBufferFD(i);
        ::ndk::ScopedFileDescriptor sfd{dup(fd)};
        auto status = ctx->getProxy()->prepareMemory(ctx->instance_id, i, sfd);
        if (!status.isOk()) {
            _log_error_with_details("prepareMemory() failed", status);
            ctx->proxy_state = aap::PLUGIN_INSTANTIATION_STATE_ERROR;
            break;
        }
    }

    if (ctx->proxy_state != aap::PLUGIN_INSTANTIATION_STATE_ERROR) {
        auto status = ctx->getProxy()->endPrepare(ctx->instance_id, buffer->num_frames);
        if (!status.isOk()) {
            _log_error_with_details("endPrepare() failed", status);
            ctx->proxy_state = aap::PLUGIN_INSTANTIATION_STATE_ERROR;
        }

		ctx->proxy_state = aap::PLUGIN_INSTANTIATION_STATE_INACTIVE;
    }
}

void aap_client_as_plugin_activate(AndroidAudioPlugin *plugin)
{
	auto ctx = (AAPClientContext*) plugin->plugin_specific;
	if (ctx->proxy_state == aap::PLUGIN_INSTANTIATION_STATE_ERROR)
		return;

    auto status = ctx->getProxy()->activate(ctx->instance_id);
    if (!status.isOk()) {
        _log_error_with_details("activate() failed", status);
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

    auto shmBuffer = ctx->shm_store->getAudioPluginBuffer();

	// FIXME: copy only input ports
	for (size_t i = 0; i < buffer->num_buffers; i++) {
		memcpy(shmBuffer->buffers[i], buffer->buffers[i], buffer->num_frames * sizeof(float));
	}

	auto status = ctx->getProxy()->process(ctx->instance_id, timeoutInNanoseconds);
    if (!status.isOk()) {
        _log_error_with_details("process() failed", status);
        ctx->proxy_state = aap::PLUGIN_INSTANTIATION_STATE_ERROR;
    }

	// FIXME: copy only output ports
	for (size_t i = 0; i < buffer->num_buffers; i++) {
		memcpy(buffer->buffers[i], shmBuffer->buffers[i], buffer->num_frames * sizeof(float));
	}
}

void aap_client_as_plugin_deactivate(AndroidAudioPlugin *plugin)
{
	auto ctx = (AAPClientContext*) plugin->plugin_specific;
	if (ctx->proxy_state == aap::PLUGIN_INSTANTIATION_STATE_ERROR)
		return;

	auto status = ctx->getProxy()->deactivate(ctx->instance_id);
    if (!status.isOk()) {
        _log_error_with_details("deactivate() failed", status);
        ctx->proxy_state = aap::PLUGIN_INSTANTIATION_STATE_ERROR;
    }

    ctx->proxy_state = aap::PLUGIN_INSTANTIATION_STATE_INACTIVE;
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
    ctx->connection_data = (aap::AndroidPluginClientConnectionData*) client->getConnections()->getServiceHandleForConnectedPlugin(pluginUniqueId);

    ctx->unique_id = pluginUniqueId;
    ctx->shm_store = std::make_unique<aap::PluginSharedMemoryStore>();

    auto status = ctx->getProxy()->beginCreate(pluginUniqueId, aapSampleRate, &ctx->instance_id);
    if (!status.isOk()) {
        _log_error_with_details("beginCreate() failed", status);
        // It will still return the plugin instance anyways, even though it is not really usable.
        ctx->proxy_state = aap::PLUGIN_INSTANTIATION_STATE_ERROR;
    } else {

        auto instance = (aap::RemotePluginInstance *) host->context;
		instance->setInstanceId(ctx->instance_id);

        // It is a nasty workaround to not expose Binder back to RemotePluginInstance; we set a callable function for them here.
        instance->send_extension_message_impl = [ctx](const char* uri,
                                                      int32_t instanceId, int32_t opcode) {
            if (ctx->proxy_state != aap::PLUGIN_INSTANTIATION_STATE_ERROR) {
                auto stat = ctx->getProxy()->extension(instanceId, uri, opcode);
                if (!stat.isOk()) {
                    _log_error_with_details("extension() failed", stat);
                    ctx->proxy_state = aap::PLUGIN_INSTANTIATION_STATE_ERROR;
                }
            }
        };

        // Set up shared memory FDs for plugin extension services.
        // We make use of plugin metadata that should list up required and optional extensions.
        if (!instance->getAAPXSManager()->setupAAPXSInstances([&](AAPXSClientInstance *ext) {
            // create asharedmem and add as an extension FD, keep it until it is destroyed.
            auto fd = ASharedMemory_create(nullptr, ext->data_size);
            ext->data = ctx->shm_store->addExtensionFD(fd, ext->data_size);

            if (ctx->proxy_state != aap::PLUGIN_INSTANTIATION_STATE_ERROR) {
                ndk::ScopedFileDescriptor sfd{dup(fd)};
                auto stat = ctx->getProxy()->addExtension(ctx->instance_id, ext->uri, sfd, ext->data_size);
                if (!stat.isOk()) {
                    _log_error_with_details("addExtension() failed", stat);
                    ctx->proxy_state = aap::PLUGIN_INSTANTIATION_STATE_ERROR;
                }
            }
        }))
            ctx->proxy_state = aap::PLUGIN_INSTANTIATION_STATE_ERROR;

        if (ctx->proxy_state != aap::PLUGIN_INSTANTIATION_STATE_ERROR) {
            status = ctx->getProxy()->endCreate(ctx->instance_id);
            if (!status.isOk()) {
                _log_error_with_details("endCreate() failed", status);
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
		aap_client_as_plugin_get_extension
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
