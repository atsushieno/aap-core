#pragma once

using namespace std;

extern "C" {

/* forward declarations */
typedef void AAPHandle;
struct AndroidAudioPlugin;
typedef struct AndroidAudioPlugin AndroidAudioPlugin;


typedef struct {
	/* NULL-terminated list of buffers */
	void **buffers;
	int32_t num_frames;
} AndroidAudioPluginBuffer;

/* They can be consolidated as a standalone helper library when people want C API for bindings (such as P/Invokes)
 */
#if 0
void **aap_buffer_get_buffers (AndroidAudioPluginBuffer *buffer) { return buffer->buffers;
int32_t aap_buffer_num_frames (AndroidAudioPluginBuffer *buffer) { return buffer->num_frames; }
#endif

/* A minimum state support is provided in AAP framework itself.
 * LV2 has State extension, but it is provided to rather guide plugin developers
 * how to implement it. AAP is rather a bridge framework and needs simple solution.
 * Anyone can develop something similar session implementation helpers.
 */
typedef struct {
	int32_t data_size;
	const void *raw_data;
} AndroidAudioPluginState;


typedef struct {
	const char *uri;
	const void *data;
} AndroidAudioPluginExtension;


/* function types */
typedef AAPHandle* (*aap_instantiate_func_t) (AndroidAudioPlugin *pluginEntry, int sampleRate, const AndroidAudioPluginExtension * const *extensions);

typedef void (*aap_prepare_func_t) (AAPHandle *pluginHandle);

typedef void (*aap_control_func_t) (AAPHandle *pluginHandle);

typedef void (*aap_process_func_t) (AAPHandle *pluginHandle, AndroidAudioPluginBuffer* audioBuffer, long timeoutInNanoseconds);

typedef const AndroidAudioPluginState* (*aap_get_state_func_t) (AAPHandle *pluginHandle);

typedef void (*aap_set_state_func_t) (AAPHandle *pluginHandle, AndroidAudioPluginState *input);

typedef struct AndroidAudioPlugin {
	aap_instantiate_func_t instantiate;
	aap_prepare_func_t prepare;
	aap_control_func_t activate;
	aap_process_func_t process;
	aap_control_func_t deactivate;
	aap_control_func_t terminate;
	aap_get_state_func_t get_state;
	aap_set_state_func_t set_state;
} AndroidAudioPlugin; // plugin implementors site

AndroidAudioPlugin* GetAndroidAudioPluginEntry ();

typedef AndroidAudioPlugin* (*aap_instantiate_t) ();

} // extern "C"

