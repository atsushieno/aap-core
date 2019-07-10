#include <sys/mman.h>
#include <cstdlib>
#include <android/sharedmem.h>
#include "../include/android-audio-plugin.h"
#include "aap/IAudioPluginService.h"

class AAPClientContext {
public:
	const char *unique_id;
	int sample_rate;
	std::shared_ptr<aidl::aap::IAudioPluginService> proxy;
	AndroidAudioPluginBuffer *previous_buffer;
	AndroidAudioPluginState state;
	int state_ashmem_fd;

	AAPClientContext(const char *uniqueId, int sampleRate, std::shared_ptr<aidl::aap::IAudioPluginService> proxy)
		: unique_id(uniqueId), sample_rate(sampleRate), proxy(proxy)
	{
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
	while (auto p = buffer->buffers[n])
		n++;
	std::vector<int64_t > pointers;
	for (int i = 0; i < n; i++)
		pointers.push_back((long) buffer->buffers[i]);
	// FIXME: status check
	ctx->proxy->prepare(buffer->num_frames, n, pointers);
	ctx->previous_buffer = buffer;
}

void aap_bridge_plugin_prepare(AndroidAudioPlugin *plugin, AndroidAudioPluginBuffer* buffer)
{
	auto ctx = (AAPClientContext*) plugin->plugin_specific;
	resetBuffers(ctx, buffer);
}

void aap_bridge_plugin_activate(AndroidAudioPlugin *plugin)
{
	auto ctx = (AAPClientContext*) plugin->plugin_specific;
	ctx->proxy->activate();
}

void aap_bridge_plugin_process(AndroidAudioPlugin *plugin,
	AndroidAudioPluginBuffer* buffer,
	long timeoutInNanoseconds)
{
	auto ctx = (AAPClientContext*) plugin->plugin_specific;
	if (ctx->previous_buffer != buffer)
		resetBuffers(ctx, buffer);
	ctx->proxy->process(timeoutInNanoseconds);
}

void aap_bridge_plugin_deactivate(AndroidAudioPlugin *plugin)
{
	auto ctx = (AAPClientContext*) plugin->plugin_specific;
	ctx->proxy->deactivate();
}

const AndroidAudioPluginState* aap_bridge_plugin_get_state(AndroidAudioPlugin *plugin)
{
	auto ctx = (AAPClientContext*) plugin->plugin_specific;
	int size;
	// FIXME: status check
	ctx->proxy->getStateSize(&size);
	ensureStateBuffer(ctx, size);
	// FIXME: status check
	// FIXME: Maybe this should be one call for potential state length mismatch.
	ctx->proxy->getState((long) &ctx->state.raw_data);
}

void aap_bridge_plugin_set_state(AndroidAudioPlugin *plugin, AndroidAudioPluginState *input)
{
	auto ctx = (AAPClientContext*) plugin->plugin_specific;
	// we have to ensure that the pointer is shared memory, so use state buffer inside ctx.
	ensureStateBuffer(ctx, input->data_size);
	memcpy((void*) ctx->state.raw_data, input->raw_data, (size_t) input->data_size);
	ctx->proxy->setState((long) input->raw_data, input->data_size);
}

void* aap_binder_on_create(void* args) { return nullptr; }
void aap_binder_on_destroy(void *userData) {}
binder_status_t aap_binder_on_transact(AIBinder *binder, transaction_code_t code, const AParcel *in, AParcel *out) { return STATUS_OK; }

AndroidAudioPlugin* aap_bridge_plugin_new(
	AndroidAudioPluginFactory *pluginFactory,
	const char* pluginUniqueId,
	int aapSampleRate,
	const AndroidAudioPluginExtension * const *extensions)
{
	AIBinder_Class *cls = AIBinder_Class_define("AudioPluginService",
												aap_binder_on_create,
												aap_binder_on_destroy,
												aap_binder_on_transact);
	auto binder = ndk::SpAIBinder(AIBinder_new(cls, nullptr));
	auto ctx = new AAPClientContext(pluginUniqueId, aapSampleRate, aidl::aap::IAudioPluginService::fromBinder(binder));
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
		AndroidAudioPluginFactory *pluginFactory,
		AndroidAudioPlugin *instance)
{
	auto ctx = (AAPClientContext*) instance->plugin_specific;
	releaseStateBuffer(ctx);

	delete instance;
}

extern "C" {

AndroidAudioPluginFactory *GetAndroidAudioPluginFactoryClientBridge() {
	return new AndroidAudioPluginFactory{aap_bridge_plugin_new, aap_bridge_plugin_delete};
}

}
