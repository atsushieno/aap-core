/*
  ==============================================================================

    android-audio-plugin-host.cpp
    Created: 9 May 2019 3:09:22am
    Author:  atsushieno

  ==============================================================================
*/

#include "aap/android-audio-plugin-host.hpp"
#include <vector>

namespace aap
{

PluginHost::PluginHost(const PluginInformation* const* pluginDescriptors)
{
	backends.push_back(&backend_lv2);
	backends.push_back(&backend_vst3);

	int n = 0;
	while (auto p = pluginDescriptors[n])
		n++;
	for (int i = 0; i < n; i++)
		plugin_descriptors.push_back(pluginDescriptors[i]);
}

bool PluginHost::isPluginAlive (const char *identifier) 
{
	auto desc = getPluginDescriptor(identifier);
	if (!desc)
		return false;

	if (desc->isOutProcess()) {
		// TODO: implement healthcheck
	} else {
		// assets won't be removed
	}

	// need more validation?
	
	return true;
}

bool PluginHost::isPluginUpToDate (const char *identifier, long lastInfoUpdated)
{
	auto desc = getPluginDescriptor(identifier);
	if (!desc)
		return false;

	return desc->getLastInfoUpdateTime() <= lastInfoUpdated;
}

PluginInstance* PluginHost::instantiatePlugin(const char *identifier)
{
	const PluginInformation *descriptor = getPluginDescriptor(identifier);
	// For local plugins, they can be directly loaded using dlopen/dlsym.
	// For remote plugins, the connection has to be established through binder.
	if (descriptor->isOutProcess())
		return instantiateRemotePlugin(descriptor);
	else
		return instantiateLocalPlugin(descriptor);
}

PluginInstance* PluginHost::instantiateLocalPlugin(const PluginInformation *descriptor)
{
	const char *file = descriptor->getLocalPluginSharedLibrary();
	const char *entrypoint = descriptor->getLocalPluginLibraryEntryPoint();
	auto dl = dlopen(file ? file : "androidaudioplugin", RTLD_LAZY);
	auto factoryGetter = (aap_factory_t) dlsym(dl, entrypoint ? entrypoint : "GetAndroidAudioPluginFactory");
	auto pluginFactory = factoryGetter();

	return new PluginInstance(this, descriptor, pluginFactory);
}

PluginInstance* PluginHost::instantiateRemotePlugin(const PluginInformation *descriptor)
{
	auto dl = dlopen("androidaudioplugin", RTLD_LAZY);
	auto factoryGetter = (aap_factory_t) dlsym(dl, "GetAndroidAudioPluginFactoryClientBridge");
	auto pluginFactory = factoryGetter();
	return new PluginInstance(this, descriptor, pluginFactory);
}


} // namespace

namespace dogfoodingaap {

void sample_plugin_delete(
	AndroidAudioPluginFactory *pluginFactory,
	AndroidAudioPlugin *instance)
{
	delete instance;
}

void sample_plugin_prepare(AndroidAudioPlugin *plugin, AndroidAudioPluginBuffer* buffer)
{
}
void sample_plugin_activate(AndroidAudioPlugin *plugin) {}
void sample_plugin_process(AndroidAudioPlugin *plugin,
	AndroidAudioPluginBuffer* buffer,
	long timeoutInNanoseconds)
{
	/* do something */
}
void sample_plugin_deactivate(AndroidAudioPlugin *plugin) {}

AndroidAudioPluginState state;
const AndroidAudioPluginState* sample_plugin_get_state(AndroidAudioPlugin *plugin)
{
	return &state; /* fill it */
}

void sample_plugin_set_state(AndroidAudioPlugin *plugin, AndroidAudioPluginState *input)
{
	/* apply argument input */
}

AndroidAudioPlugin* sample_plugin_new(
	AndroidAudioPluginFactory *pluginFactory,
	const char* pluginUniqueId,
	int sampleRate,
	const AndroidAudioPluginExtension * const *extensions)
{
	return new AndroidAudioPlugin {
		NULL,
		sample_plugin_prepare,
		sample_plugin_activate,
		sample_plugin_process,
		sample_plugin_deactivate,
		sample_plugin_get_state,
		sample_plugin_set_state
		};
}

AndroidAudioPluginFactory* GetAndroidAudioPluginFactory ()
{
	return new AndroidAudioPluginFactory { sample_plugin_new, sample_plugin_delete };
}


void dogfooding_api()
{
	aap::PluginHost manager(NULL);
	auto paths = manager.instantiatePlugin(NULL);
	GetAndroidAudioPluginFactory();
}


} // namespace dogfoodingaap
