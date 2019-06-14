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

typedef struct {
	void **buffers;
	int32_t numBuffers;
	int32_t numFrames;
} AndroidAudioPluginBuffer;


int32_t aap_buffer_num_buffers (AndroidAudioPluginBuffer *buffer);

void **aap_buffer_get_buffers (AndroidAudioPluginBuffer *buffer);

int32_t aap_buffer_num_frames (AndroidAudioPluginBuffer *buffer);


typedef void (*aap_process_func_t) (AndroidAudioPluginBuffer* audioBuffer, AndroidAudioPluginBuffer* controlBuffer, long timeoutInNanoseconds);

typedef struct {
	aap_process_func_t process;
} AndroidAudioPlugin; // plugin implementors site

AndroidAudioPlugin* GetAndroidAudioPluginEntry ();

typedef AndroidAudioPlugin* (*aap_instantiate_t) ();

} // extern "C"

