#include "aap/android-audio-plugin.h"
#include "aap/core/host/audio-plugin-host.h"
#include "aap/core/host/plugin-client-system.h"
#include "aap/core/host/desktop/audio-plugin-host-desktop.h"
#include "aap/core/aapxs/extension-service.h"

#if !ANDROID

extern "C"
int32_t getMidiSettingsFromLocalConfig(std::string pluginId) {
	assert(false); // FIXME: implement
}

extern "C"
int32_t createGui(std::string pluginId, int32_t instanceId, void* audioPluginView) {
	assert(false); // It is not implemented (not even suposed to be).
}

namespace aap {

PluginClientSystem* PluginClientSystem::getInstance() {
	assert(false); // FIXME: implement
}

} // namespace aap

AndroidAudioPlugin* aap_desktop_plugin_new(
	AndroidAudioPluginFactory *pluginFactory,
	const char* pluginUniqueId,
	int aapSampleRate,
	AndroidAudioPluginHost* host
	)
{
	assert(false); // FIXME: implement
}

void aap_desktop_plugin_delete(
		AndroidAudioPluginFactory *pluginFactory,
		AndroidAudioPlugin *instance)
{
	assert(false); // FIXME: implement
}

AndroidAudioPluginFactory *GetDesktopAudioPluginFactoryClientBridge(aap::PluginClient* client) {
    return new AndroidAudioPluginFactory{aap_desktop_plugin_new, aap_desktop_plugin_delete, client};
}

#endif
