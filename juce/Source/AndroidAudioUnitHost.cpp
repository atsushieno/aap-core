/*
  ==============================================================================

    AndroidAudioUnit.cpp
    Created: 9 May 2019 3:09:22am
    Author:  atsushieno

  ==============================================================================
*/

#include "AndroidAudioUnitHost.h"
#include "android/asset_manager.h"
#include <vector>
#include <dirent.h>

using namespace std::filesystem;

extern "C" {

int32_t android_audio_plugin_buffer_num_buffers (AndroidAudioPluginBuffer *buffer)
{
	return buffer->numBuffers;
}

void **android_audio_plugin_buffer_get_buffers (AndroidAudioPluginBuffer *buffer)
{
	return buffer->buffers;
}

int32_t android_audio_plugin_buffer_num_frames (AndroidAudioPluginBuffer *buffer)
{
	return buffer->numFrames;
}

} // extern "C"

namespace aap
{

AndroidAudioPluginDescriptor* AndroidAudioPluginManager::loadDescriptorFromBundleDirectory(const char *directory)
{
	// FIXME: implement
}

void AndroidAudioPluginManager::updatePluginDescriptorList(const char **searchPaths, bool recursive, bool asynchronousInstantiationAllowed)
{
	// FIXME: support recursive
	vector<AndroidAudioPluginDescriptor*> descs;	
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

bool AndroidAudioPluginManager::isPluginAlive (const char *identifier) 
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

bool AndroidAudioPluginManager::isPluginUpToDate (const char *identifier, long lastInfoUpdated)
{
	auto desc = getPluginDescriptor(identifier);
	if (!desc)
		return false;
	auto dir = opendir(desc->getFilePath());
	if (dir == NULL)
		return false;

	// FIXME: implement
}

AndroidAudioPluginInstance* AndroidAudioPluginManager::instantiatePlugin(AndroidAudioPluginDescriptor *descriptor)
{

	// FIXME: implement correctly
	const char *file = descriptor->getFilePath();
	auto dl = dlopen(file, RTLD_LAZY);
	auto getter = (android_audio_plugin_instantiate_t) dlsym(dl, "GetAndroidAudioPluginEntry");
	auto plugin = getter();

	return new AndroidAudioPluginInstance(descriptor, plugin);
}


void dogfooding_api()
{
	AndroidAudioPluginManager manager;
	auto paths = manager.getDefaultPluginSearchPaths();
	
}

} // namespace
