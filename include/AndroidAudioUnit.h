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

typedef struct {
	const char* uri;
	void *data;
} AndroidAudioPluginExtension;

/* They can be consolidated as a standalone helper library when people want C API for bindings (such as P/Invokes)
 */
#if 0
int32_t aap_buffer_num_buffers (AndroidAudioPluginBuffer *buffer);
void **aap_buffer_get_buffers (AndroidAudioPluginBuffer *buffer);
int32_t aap_buffer_num_frames (AndroidAudioPluginBuffer *buffer);
#endif

typedef void AAPHandle;
struct AndroidAudioPlugin;
typedef struct AndroidAudioPlugin AndroidAudioPlugin;

typedef AAPHandle* (*aap_instantiate_func_t) (AndroidAudioPlugin *pluginEntry, int sampleRate, const AndroidAudioPluginExtension * const *extensions);

typedef void (*aap_prepare_func_t) (AAPHandle *pluginHandle, AndroidAudioPluginBuffer* audioBufferHint, AndroidAudioPluginBuffer* controlBufferHint);

typedef void (*aap_control_func_t) (AAPHandle *pluginHandle);

typedef void (*aap_process_func_t) (AAPHandle *pluginHandle, AndroidAudioPluginBuffer* audioBuffer, AndroidAudioPluginBuffer* controlBuffer, long timeoutInNanoseconds);

typedef struct AndroidAudioPlugin {
	aap_instantiate_func_t instantiate;
	aap_prepare_func_t prepare;
	aap_control_func_t activate;
	aap_process_func_t process;
	aap_control_func_t deactivate;
	aap_control_func_t terminate;
} AndroidAudioPlugin; // plugin implementors site

AndroidAudioPlugin* GetAndroidAudioPluginEntry ();

typedef AndroidAudioPlugin* (*aap_instantiate_t) ();

} // extern "C"

