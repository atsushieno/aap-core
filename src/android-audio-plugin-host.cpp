/*
  ==============================================================================

    AndroidAudioUnit.cpp
    Created: 9 May 2019 3:09:22am
    Author:  atsushieno

  ==============================================================================
*/

#include "../include/android-audio-plugin-host.hpp"
#include <vector>

namespace aap
{

PluginHost::PluginHost(const PluginInformation* const* pluginDescriptors)
{
	backends.push_back(&backend_lv2);
	backends.push_back(&backend_vst3);

	int n = 0;
	for (auto p = pluginDescriptors; p; p++)
		n++;
	for (int i = 0; i < n; i++)
		plugin_descriptors.push_back(pluginDescriptors[i]);
}

PluginInformation* PluginHost::loadDescriptorFromAssetBundleDirectory(const char *directory)
{
	// TODO: implement. load AAP manifest and fill descriptor.
	
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
		instantiateRemotePlugin(descriptor);
	else
		instantiateLocalPlugin(descriptor);
}

PluginInstance* PluginHost::instantiateLocalPlugin(const PluginInformation *descriptor)
{
	const char *file = descriptor->getLocalPluginSharedLibrary();
	auto dl = dlopen(file, RTLD_LAZY);
	auto factoryGetter = (aap_factory_t) dlsym(dl, "GetAndroidAudioPluginFactory");
	auto pluginFactory = factoryGetter();

	return new PluginInstance(this, descriptor, pluginFactory);
}

PluginInstance* PluginHost::instantiateRemotePlugin(const PluginInformation *descriptor)
{
	// TODO: implement. remote instantiation
	assert(false);
}


void dogfooding_api()
{
	PluginHost manager(NULL);
	auto paths = manager.instantiatePlugin(NULL);
	
}

} // namespace
