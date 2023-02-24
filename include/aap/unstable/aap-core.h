#pragma once

#ifndef AAP_CORE_H_INCLUDED
#define AAP_CORE_H_INCLUDED

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

// FIXME: there should be some definition for this constant elsewhere
#define DEFAULT_CONTROL_BUFFER_SIZE 8192

typedef struct aap_buffer_t {
	/** implementation details that only creator of this struct should touch */
	void* impl;

	/** Number of frames in the audio buffer. It does not apply to non-audio buffers. */
	int32_t (*num_frames)(aap_buffer_t& self);

	/** Number of buffers. */
	int32_t (*num_ports)(aap_buffer_t& self);

	/**
     * The pointer to the audio buffer. It may or may not be the shared pointer betwee host and client.
     * It may return null.*/
	void * (*get_buffer)(aap_buffer_t& self, int32_t index);

	/**
     * The default size of buffers can be calculated by num_frames * sizeof(float) or num_frames * sizeof (double),
     * but note that they can be controlled by some port parameters such as `minimumSize` property. */
	int32_t (*get_buffer_size)(aap_buffer_t& self, int32_t index);
} aap_buffer_t;

const int32_t MAX_PLUGIN_ID_SIZE = 1024; // FIXME: there should be some official definition.

/* forward declarations */
struct AndroidAudioPluginFactory;
struct AndroidAudioPlugin;

struct AndroidAudioPluginHost;

typedef void* (*aap_host_get_extension_data_t) (struct AndroidAudioPluginHost* host, const char *uri);

/**
 * Represents a host from plugin's perspective.
 * AAP client host is supposed to provide it (aap::PluginClient does so).
 * Note that it is not to represent a comprehensive host that manages multiple instances, but
 * to provide minimum information that is exposed to plugin.
 */
typedef struct AndroidAudioPluginHost {
	void *context;
	/** returns the shared data space that the host provides. */
	aap_host_get_extension_data_t get_extension_data;
} AndroidAudioPluginHost;

/* function types */
typedef struct AndroidAudioPlugin* (*aap_instantiate_func_t) (
	struct AndroidAudioPluginFactory *pluginFactory,
	const char* pluginUniqueId,
	int sampleRate,
	AndroidAudioPluginHost *host);

typedef void (*aap_release_func_t) (
	struct AndroidAudioPluginFactory *pluginFactory,
	struct AndroidAudioPlugin *instance);

typedef void (*aap_prepare_func_t) (
	struct AndroidAudioPlugin *plugin,
	aap_buffer_t* audioBuffer);

typedef void (*aap_control_func_t) (struct AndroidAudioPlugin *plugin);

typedef void (*aap_process_func_t) (
	struct AndroidAudioPlugin *plugin,
	aap_buffer_t* audioBuffer,
	long timeoutInNanoseconds);

typedef void* (*aap_get_extension_func_t) (
		struct AndroidAudioPlugin *plugin,
		const char *uri);

typedef struct AndroidAudioPlugin {
	/** Plugin can store plugin-specific data here.
	 * Host should not alter it.
	 * Framework does not touch it. */
	void *plugin_specific;

	aap_prepare_func_t prepare;
	/** Changes the prepared plugin's state as enabled for audio processing.
	 * It has to finish in realtime manner.
	 * Plugin should implement it.
	 * Host calls it.
	 */
	aap_control_func_t activate;

	/** Instructs the pluing to process a set of input buffers and produce outputs to the output buffers.
	 * It has to finish in realtime manner.
	 * Plugin should implement it.
	 * Host calls it.
	 */
	aap_process_func_t process;

	/** Changes the prepared plugin's state as disabled for audio processing.
	 * It has to finish in realtime manner.
	 * Plugin should implement it.
	 * Host calls it.
	 */
	aap_control_func_t deactivate;

	/** Plugin has to implement this function to return any extension that the plugin supports. */
	aap_get_extension_func_t get_extension;
} AndroidAudioPlugin;

/**
 * It is used by the Standard Extensions that need the target AndroidAudioPlugin.
 */
typedef struct AndroidAudioPluginExtensionTarget {
    /** The target plugin. An extension implementation usually need the target plugin to process.
     * Extensions also have to come up with their Extension Service i.e. AAPXS, which works as a
     * bridge between the host and the plugin, and AAPXS needs its host context to process.
     * Those Standard Extensions that need plugin information take a pointer to this struct as their first
     */
    AndroidAudioPlugin *plugin;
    /**
     * AAPXS-specific data. It will be Instance in presets extension and state extension.
     * It is only for remote clients. For local plugin processing, it is set to nullptr.
     */
    void *aapxs_context;
} AndroidAudioPluginExtensionTarget;

typedef struct AndroidAudioPluginFactory {
	aap_instantiate_func_t instantiate;
	aap_release_func_t release;
	void *factory_context;
} AndroidAudioPluginFactory;

/* All plugin developers are supposed to implement this function, either directly or via some SDKs. */
AndroidAudioPluginFactory* GetAndroidAudioPluginFactory ();

typedef AndroidAudioPluginFactory* (*aap_factory_t) ();

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* AAP_CORE_H_INCLUDED */
