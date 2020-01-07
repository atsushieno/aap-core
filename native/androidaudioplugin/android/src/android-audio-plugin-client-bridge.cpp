
#include <sys/mman.h>
#include <cstdlib>
#include <android/sharedmem.h>
#include <android/log.h>
#include <aidl/org/androidaudioplugin/BpAudioPluginInterface.h>
#include <aidl/org/androidaudioplugin/BnAudioPluginInterface.h>
#include "aap/android-audio-plugin.h"
#include "AudioPluginInterfaceImpl.h"

class AAPClientContext {
public:
	const char *unique_id;
	std::unique_ptr<aap::AudioPluginInterfaceImpl> impl{nullptr};
	std::shared_ptr<aidl::org::androidaudioplugin::IAudioPluginInterface> proxy;
	AndroidAudioPluginBuffer *previous_buffer;
	AndroidAudioPluginState state;
	int state_ashmem_fd;

    AIBinder_Class_onTransact on_transact = [](AIBinder *binder,
            transaction_code_t code, const AParcel *in, AParcel *out)
                    { return (binder_status_t) 0; };

    AAPClientContext(int sampleRate, const char *pluginUniqueId)
		: unique_id(pluginUniqueId)
	{
        impl.reset(new aap::AudioPluginInterfaceImpl(sampleRate));
		proxy = aidl::org::androidaudioplugin::BpAudioPluginInterface::fromBinder(impl->asBinder());
    }

    ~AAPClientContext()
    {
	    proxy->destroy();
    }
};

void releaseStateBuffer(AAPClientContext *ctx)
{
	if (ctx->state.raw_data)
		munmap((void*) ctx->state.raw_data, (size_t) ctx->state.data_size);
	if (ctx->state_ashmem_fd)
		close(ctx->state_ashmem_fd);
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
	int n = 0;
	while (auto p = buffer->buffers[n]) {
		::ndk::ScopedFileDescriptor fd;
		fd.set((int64_t) p);
		auto pmstatus = ctx->proxy->prepareMemory(n, fd);
		assert (pmstatus.isOk());
		n++;
	}
	auto status = ctx->proxy->prepare(buffer->num_frames, n);
    assert (status.isOk());
	ctx->previous_buffer = buffer;
}

void aap_bridge_plugin_prepare(AndroidAudioPlugin *plugin, AndroidAudioPluginBuffer* buffer)
{
	__android_log_print(ANDROID_LOG_DEBUG, "!!!AAPDEBUG!!!", "aap_bridge_plugin_prepare invoked");
	auto ctx = (AAPClientContext*) plugin->plugin_specific;
	resetBuffers(ctx, buffer);
}

void aap_bridge_plugin_activate(AndroidAudioPlugin *plugin)
{
	auto ctx = (AAPClientContext*) plugin->plugin_specific;
    auto status = ctx->proxy->activate();
    assert (status.isOk());
}

void aap_bridge_plugin_process(AndroidAudioPlugin *plugin,
	AndroidAudioPluginBuffer* buffer,
	long timeoutInNanoseconds)
{
	auto ctx = (AAPClientContext*) plugin->plugin_specific;
	if (ctx->previous_buffer != buffer)
		resetBuffers(ctx, buffer);
	auto status = ctx->proxy->process(timeoutInNanoseconds);
    assert (status.isOk());
}

void aap_bridge_plugin_deactivate(AndroidAudioPlugin *plugin)
{
	auto ctx = (AAPClientContext*) plugin->plugin_specific;
	auto status = ctx->proxy->deactivate();
    assert (status.isOk());
}

const AndroidAudioPluginState* aap_bridge_plugin_get_state(AndroidAudioPlugin *plugin)
{
	auto ctx = (AAPClientContext*) plugin->plugin_specific;
	int size;
	auto status = ctx->proxy->getStateSize(&size);
    assert (status.isOk());
	ensureStateBuffer(ctx, size);
	// FIXME: status check
	// FIXME: Maybe this should be one call for potential state length mismatch.
	::ndk::ScopedFileDescriptor fd;
	fd.set((int64_t) &ctx->state.raw_data);
	auto status2 = ctx->proxy->getState(fd);
    assert (status2.isOk());

}

void aap_bridge_plugin_set_state(AndroidAudioPlugin *plugin, AndroidAudioPluginState *input)
{
	auto ctx = (AAPClientContext*) plugin->plugin_specific;
	// we have to ensure that the pointer is shared memory, so use state buffer inside ctx.
	ensureStateBuffer(ctx, input->data_size);
	memcpy((void*) ctx->state.raw_data, input->raw_data, (size_t) input->data_size);
	::ndk::ScopedFileDescriptor fd;
	fd.set((int64_t) input->raw_data);
	auto status = ctx->proxy->setState(fd, input->data_size);
    assert (status.isOk());
}

AndroidAudioPlugin* aap_bridge_plugin_new(
	AndroidAudioPluginFactory *pluginFactory,	// unused
	const char* pluginUniqueId,
	int aapSampleRate,
	const AndroidAudioPluginExtension * const *extensions	// unused
	)
{
	assert(pluginFactory != nullptr);
	assert(pluginUniqueId != nullptr);

	auto ctx = new AAPClientContext(aapSampleRate, pluginUniqueId);
	auto status = ctx->proxy->create(pluginUniqueId, aapSampleRate);
    assert (status.isOk());
	return new AndroidAudioPlugin {
		ctx,
		aap_bridge_plugin_prepare,
		aap_bridge_plugin_activate,
		aap_bridge_plugin_process,
		aap_bridge_plugin_deactivate,
		aap_bridge_plugin_get_state,
		aap_bridge_plugin_set_state
		};
}

void aap_bridge_plugin_delete(
		AndroidAudioPluginFactory *pluginFactory,	// unused
		AndroidAudioPlugin *instance)
{
	auto ctx = (AAPClientContext*) instance->plugin_specific;
	releaseStateBuffer(ctx);

	delete ctx;
	delete instance;
}

extern "C" {

AndroidAudioPluginFactory *GetAndroidAudioPluginFactoryClientBridge() {
	return new AndroidAudioPluginFactory{aap_bridge_plugin_new, aap_bridge_plugin_delete};
}

}
