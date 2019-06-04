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

void AndroidAudioPluginManager::updatePluginDescriptorList(const char **searchPaths, bool recursive, bool asynchronousInstantiationAllowed)
{
	// FIXME: support recursive
	vector<const char*> files;	
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
				files.push_back(strdup(de->d_name));
		}
		closedir(dir);
	}
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

} // namespace
