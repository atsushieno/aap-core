/*
  ==============================================================================

    AndroidAudioUnit.cpp
    Created: 9 May 2019 3:09:22am
    Author:  atsushieno

  ==============================================================================
*/

#include "AndroidAudioUnit.h"
#include "android/asset_manager.h"

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

AndroidAudioPlugin* AndroidAudioPluginManager::instantiatePlugin(AndroidAudioPluginDescriptor *descriptor)
{
	return new AndroidAudioPlugin(descriptor);
}

} // namespace
