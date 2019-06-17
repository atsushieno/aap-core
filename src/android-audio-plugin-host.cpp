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
#include <dirent.h>

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

AAPDescriptor* AAPHost::loadDescriptorFromBundleDirectory(const char *directory)
{
	// FIXME: implement
}

void AAPHost::updatePluginDescriptorList(const char **searchPaths, bool recursive, bool asynchronousInstantiationAllowed)
{
	// FIXME: support recursive
	vector<AAPDescriptor*> descs;	
	for (int i = 0; searchPaths[i]; i++) {
		auto dir = opendir(searchPaths[i]);
		if (dir == NULL)
			continue; // for whatever reason, failed to open directory.
		while (true) {
			auto de = readdir(dir);
			if (de == NULL)
				break;
			auto len = strlen(de->d_name);
			if (strncmp(((char*) de->d_name) + len - 3, ".aap", 3) == 0)
				descs.push_back(loadDescriptorFromBundleDirectory(de->d_name));
		}
		closedir(dir);
	}
}

bool AAPHost::isPluginAlive (const char *identifier) 
{
	auto desc = getPluginDescriptor(identifier);
	if (!desc)
		return false;
	auto dir = opendir(desc->getFilePath());
	if (dir == NULL)
		return false;
	
	// need more validation?
	
	closedir(dir);
	return true;
}

bool AAPHost::isPluginUpToDate (const char *identifier, long lastInfoUpdated)
{
	auto desc = getPluginDescriptor(identifier);
	if (!desc)
		return false;
	auto dir = opendir(desc->getFilePath());
	if (dir == NULL)
		return false;

	// FIXME: implement
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
