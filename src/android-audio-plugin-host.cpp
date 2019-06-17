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

extern "C" {

int32_t aap_buffer_num_buffers (AndroidAudioPluginBuffer *buffer)
{
	return buffer->numBuffers;
}

void **aap_buffer_get_buffers (AndroidAudioPluginBuffer *buffer)
{
	return buffer->buffers;
}

int32_t aap_buffer_num_frames (AndroidAudioPluginBuffer *buffer)
{
	return buffer->numFrames;
}

} // extern "C"

namespace aap
{

void AAPHost::initialize(AAssetManager *assetManager, const char* const *pluginAssetDirectories)
{
	asset_manager = assetManager;
	vector<AAPDescriptor*> descs;
	for (int i = 0; pluginAssetDirectories[i]; i++) {
		auto dir = AAssetManager_openDir(assetManager, pluginAssetDirectories[i]);
		if (dir == NULL)
			continue; // for whatever reason, failed to open directory.
		descs.push_back(loadDescriptorFromBundleDirectory(pluginAssetDirectories[i]));
		AAssetDir_close(dir);
	}
	plugin_descriptors = (AAPDescriptor**) malloc(sizeof(AAPDescriptor*) * descs.size());
	std::copy(descs.begin(), descs.end(), plugin_descriptors);
}

AAPDescriptor* AAPHost::loadDescriptorFromBundleDirectory(const char *directory)
{
	// FIXME: implement
}

void AAPHost::updatePluginDescriptorList(const char **searchPaths, bool recursive, bool asynchronousInstantiationAllowed)
{
	// can't do anything. Android asset manager does not support directory iterators, 
	// and loading local file system doesn't make sense.
}

bool AAPHost::isPluginAlive (const char *identifier) 
{
	auto desc = getPluginDescriptor(identifier);
	if (!desc)
		return false;

	// assets won't be removed

	// need more validation?
	
	return true;
}

bool AAPHost::isPluginUpToDate (const char *identifier, long lastInfoUpdated)
{
	auto desc = getPluginDescriptor(identifier);
	if (!desc)
		return false;

	return desc->getLastInfoUpdateTime() <= lastInfoUpdated;
}

AAPInstance* AAPHost::instantiatePlugin(AAPDescriptor *descriptor)
{
	// For local plugins, they can be directly loaded using dlopen/dlsym.
	// For remote plugins, the connection has to be established through binder.
	if (descriptor->isOutProcess())
		instantiateRemotePlugin(descriptor);
	else
		instantiateLocalPlugin(descriptor);
}

AAPInstance* AAPHost::instantiateLocalPlugin(AAPDescriptor *descriptor)
{
	const char *file = descriptor->getFilePath();
	auto dl = dlopen(file, RTLD_LAZY);
	auto getter = (aap_instantiate_t) dlsym(dl, "GetAndroidAudioPluginEntry");
	auto plugin = getter();

	return new AAPInstance(descriptor, plugin);
}

AAPInstance* AAPHost::instantiateRemotePlugin(AAPDescriptor *descriptor)
{
	// FIXME: implement.
	assert(false);
}


void dogfooding_api()
{
	AAPHost manager;
	auto paths = manager.getDefaultPluginSearchPaths();
	
}

} // namespace
