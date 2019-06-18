/*
  ==============================================================================

    AndroidAudioUnit.cpp
    Created: 9 May 2019 3:09:22am
    Author:  atsushieno

  ==============================================================================
*/

#include "../include/android-audio-plugin-host.hpp"
#include "android/asset_manager.h"
#include <vector>

namespace aap
{

void Host::initialize(AAssetManager *assetManager, const char* const *pluginAssetDirectories)
{
	// local plugins
	asset_manager = assetManager;
	vector<PluginInformation*> descs;
	for (int i = 0; pluginAssetDirectories[i]; i++) {
		auto dir = AAssetManager_openDir(assetManager, pluginAssetDirectories[i]);
		if (dir == NULL)
			continue; // for whatever reason, failed to open directory.
		descs.push_back(loadDescriptorFromAssetBundleDirectory(pluginAssetDirectories[i]));
		AAssetDir_close(dir);
	}
	plugin_descriptors = (PluginInformation**) malloc(sizeof(PluginInformation*) * descs.size());
	
	// TODO: implement remote plugin query and store results into `descs`
	

	// done
	std::copy(descs.begin(), descs.end(), plugin_descriptors);
}

PluginInformation* Host::loadDescriptorFromAssetBundleDirectory(const char *directory)
{
	// TODO: implement. load AAP manifest and fill descriptor.
}

bool Host::isPluginAlive (const char *identifier) 
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

bool Host::isPluginUpToDate (const char *identifier, long lastInfoUpdated)
{
	auto desc = getPluginDescriptor(identifier);
	if (!desc)
		return false;

	return desc->getLastInfoUpdateTime() <= lastInfoUpdated;
}

PluginInstance* Host::instantiatePlugin(const char *identifier)
{
	PluginInformation *descriptor = getPluginDescriptor(identifier);
	// For local plugins, they can be directly loaded using dlopen/dlsym.
	// For remote plugins, the connection has to be established through binder.
	if (descriptor->isOutProcess())
		instantiateRemotePlugin(descriptor);
	else
		instantiateLocalPlugin(descriptor);
}

PluginInstance* Host::instantiateLocalPlugin(PluginInformation *descriptor)
{
	const char *file = descriptor->getLocalPluginSharedLibrary();
	auto dl = dlopen(file, RTLD_LAZY);
	auto getter = (aap_instantiate_t) dlsym(dl, "GetAndroidAudioPluginEntry");
	auto plugin = getter();

	return new PluginInstance(this, descriptor, plugin);
}

PluginInstance* Host::instantiateRemotePlugin(PluginInformation *descriptor)
{
	// TODO: implement. remote instantiation
	assert(false);
}


void dogfooding_api()
{
	Host manager;
	auto paths = manager.getDefaultPluginSearchPaths();
	
}

} // namespace
