/*
  ==============================================================================

    AndroidAudioUnit.h
    Created: 9 May 2019 3:13:10am
    Author:  atsushieno

  ==============================================================================
*/

#pragma once

using namespace std;

extern "C" {

//typedef int32_t (*android_audio_plugin_processor_func_t) (AndroidAudioPluginBuffer *audioBuffer, AndroidAudioPluginBuffer *controlBuffer, 	int64_t timeoutInNanoseconds);

typedef struct {
	void **buffers;
	int32_t numBuffers;
	int32_t numFrames;
} AndroidAudioPluginBuffer;


int32_t android_audio_plugin_buffer_num_buffers (AndroidAudioPluginBuffer *buffer);

void **android_audio_plugin_buffer_get_buffers (AndroidAudioPluginBuffer *buffer);

int32_t android_audio_plugin_buffer_num_frames (AndroidAudioPluginBuffer *buffer);


typedef void (*android_audio_plugin_process_func_t) (AndroidAudioPluginBuffer* audioBuffer, AndroidAudioPluginBuffer* controlBuffer, long timeoutInNanoseconds);

typedef struct {
	android_audio_plugin_process_func_t process;
} AndroidAudioPlugin; // plugin implementors site

AndroidAudioPlugin* GetAndroidAudioPluginEntry ();

typedef AndroidAudioPlugin* (*android_audio_plugin_instantiate_t) ();

} // extern "C"


