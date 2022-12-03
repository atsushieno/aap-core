
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
#include "AudioPluginNative_jni.h"

// AAP plugin implementation that performs actual work via AAP binder client.

class AAPClientContext {

public:
	AIBinder* binder{nullptr};
	const char *unique_id{nullptr};
	int32_t instance_id{0};
	ndk::SpAIBinder spAIBinder{nullptr};
	std::unique_ptr<aap::PluginSharedMemoryStore> shm_store{nullptr};
	std::shared_ptr<aidl::org::androidaudioplugin::IAudioPluginInterface> proxy{nullptr};
	aap_state_extension_t state_ext;
	aap_state_t state{};
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
	if (ctx->state.data)
		munmap((void*) ctx->state.data, (size_t) ctx->state.data_size);
	if (ctx->state_ashmem_fd)
		close(ctx->state_ashmem_fd);
}


AAPClientContext::~AAPClientContext() {
	if (instance_id >= 0) {
		releaseStateBuffer(this);
		proxy->destroy(instance_id);
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

void aap_client_as_plugin_prepare(AndroidAudioPlugin *plugin, AndroidAudioPluginBuffer* buffer)
{
	auto ctx = (AAPClientContext*) plugin->plugin_specific;
	if (ctx->proxy_state == aap::PLUGIN_INSTANTIATION_STATE_ERROR)
		return;

    int n = buffer->num_buffers;

	assert(ctx->shm_store->getAudioPluginBuffer()->num_buffers == 0);
    auto code = ctx->shm_store->allocateClientBuffer(buffer->num_buffers, buffer->num_frames);
	if (code != aap::PluginSharedMemoryStore::PluginMemoryAllocatorResult::PLUGIN_MEMORY_ALLOCATOR_SUCCESS) {
		aap::a_log(AAP_LOG_LEVEL_ERROR, "AAP", getMemoryAllocationErrorMessage(code));
		ctx->proxy_state = aap::PLUGIN_INSTANTIATION_STATE_ERROR;
		return;
	}

	if (ctx->proxy_state != aap::PLUGIN_INSTANTIATION_STATE_ERROR) {
		auto status = ctx->proxy->beginPrepare(ctx->instance_id);
		if (!status.isOk()) {
			aap::a_log_f(AAP_LOG_LEVEL_ERROR, "AAP.proxy",
						 "beginPrepare() failed: %s", /*status.getDescription().c_str()*/
						 "(FIXME: due to Android SDK/NDK issue 219987524 we cannot retrieve failure details here)");
			ctx->proxy_state = aap::PLUGIN_INSTANTIATION_STATE_ERROR;
		}
	}

    // allocate shm FDs, first locally, then send it to the target AAP.
    for (int i = 0; i < n; i++) {
		auto fd = ctx->shm_store->getPortBufferFD(i);
        ::ndk::ScopedFileDescriptor sfd{dup(fd)};
        auto status = ctx->proxy->prepareMemory(ctx->instance_id, i, sfd);
        if (!status.isOk()) {
            aap::a_log_f(AAP_LOG_LEVEL_ERROR, "AAP.proxy", "prepareMemory() failed: %s", /*status.getDescription().c_str()*/"(FIXME: due to Android SDK/NDK issue 219987524 we cannot retrieve failure details here)");
            ctx->proxy_state = aap::PLUGIN_INSTANTIATION_STATE_ERROR;
            break;
        }
    }

    if (ctx->proxy_state != aap::PLUGIN_INSTANTIATION_STATE_ERROR) {
        auto status = ctx->proxy->endPrepare(ctx->instance_id, buffer->num_frames);
        if (!status.isOk()) {
            aap::a_log_f(AAP_LOG_LEVEL_ERROR, "AAP.proxy", "endPrepare() failed: %s", /*status.getDescription().c_str()*/"(FIXME: due to Android SDK/NDK issue 219987524 we cannot retrieve failure details here)");
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

    auto shmBuffer = ctx->shm_store->getAudioPluginBuffer();

	// FIXME: copy only input ports
	for (size_t i = 0; i < buffer->num_buffers; i++) {
		memcpy(shmBuffer->buffers[i], buffer->buffers[i], buffer->num_frames * sizeof(float));
	}

	auto status = ctx->proxy->process(ctx->instance_id, timeoutInNanoseconds);
    if (!status.isOk()) {
        aap::a_log_f(AAP_LOG_LEVEL_ERROR, "AAP.proxy", "process() failed: %s", /*status.getDescription().c_str()*/"(FIXME: due to Android SDK/NDK issue 219987524 we cannot retrieve failure details here)");
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

	auto status = ctx->proxy->deactivate(ctx->instance_id);
    if (!status.isOk()) {
        aap::a_log_f(AAP_LOG_LEVEL_ERROR, "AAP.proxy", "deactivate() failed: %s", /*status.getDescription().c_str()*/"(FIXME: due to Android SDK/NDK issue 219987524 we cannot retrieve failure details here)");
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
    ctx->binder = (AIBinder*) client->getConnections()->getServiceHandleForConnectedPlugin(pluginUniqueId);

	if(ctx->initialize(aapSampleRate, pluginUniqueId))
		return nullptr;
    ctx->shm_store = std::make_unique<aap::PluginSharedMemoryStore>();

    auto status = ctx->proxy->beginCreate(pluginUniqueId, aapSampleRate, &ctx->instance_id);
    if (!status.isOk()) {
        // FIXME: apply this logging condition everywhere...
#ifdef __ANDROID_UNAVAILABLE_SYMBOLS_ARE_WEAK__
		if (__builtin_available(android 30, *)) {
#else
		if (__ANDROID_API__ >= 30) {
#endif
			aap::a_log_f(AAP_LOG_LEVEL_ERROR, "AAP.proxy", "beginCreate() failed: %s",
						 status.getDescription().c_str());
		} else {
			aap::a_log_f(AAP_LOG_LEVEL_ERROR, "AAP.proxy", "beginCreate() failed");
		}
        // It will still return the plugin instance anyways, even though it is not really usable.
        ctx->proxy_state = aap::PLUGIN_INSTANTIATION_STATE_ERROR;
    } else {

        auto instance = (aap::RemotePluginInstance *) host->context;

        // It is a nasty workaround to not expose Binder back to RemotePluginInstance; we set a callable function for them here.
        instance->send_extension_message_impl = [ctx](const char* uri,
                                                      int32_t instanceId, int32_t opcode) {
            if (ctx->proxy_state != aap::PLUGIN_INSTANTIATION_STATE_ERROR) {
                auto stat = ctx->proxy->extension(instanceId, uri, opcode);
                if (!stat.isOk()) {
                    aap::a_log_f(AAP_LOG_LEVEL_ERROR, "AAP.proxy", "extension() failed: %s", /*stat.getDescription().c_str()*/"(FIXME: due to Android SDK/NDK issue 219987524 we cannot retrieve failure details here)");
                    ctx->proxy_state = aap::PLUGIN_INSTANTIATION_STATE_ERROR;
                }
            }
        };

        // Set up shared memory FDs for plugin extension services.
        // We make use of plugin metadata that should list up required and optional extensions.
        if (!instance->getAAPXSManager()->setupAAPXSInstances([&](AAPXSClientInstance *ext) {
            // create asharedmem and add as an extension FD, keep it until it is destroyed.
            auto fd = ASharedMemory_create(ext->uri, ext->data_size);
            ext->data = ctx->shm_store->addExtensionFD(fd, ext->data_size);

            if (ctx->proxy_state != aap::PLUGIN_INSTANTIATION_STATE_ERROR) {
                ndk::ScopedFileDescriptor sfd{dup(fd)};
                auto stat = ctx->proxy->addExtension(ctx->instance_id, ext->uri, sfd, ext->data_size);
                if (!stat.isOk()) {
                    aap::a_log_f(AAP_LOG_LEVEL_ERROR, "AAP.proxy", "addExtension() failed: %s", /*stat.getDescription().c_str()*/"(FIXME: due to Android SDK/NDK issue 219987524 we cannot retrieve failure details here)");
                    ctx->proxy_state = aap::PLUGIN_INSTANTIATION_STATE_ERROR;
                }
            }
        }))
            ctx->proxy_state = aap::PLUGIN_INSTANTIATION_STATE_ERROR;

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
