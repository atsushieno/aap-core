#pragma once

#ifndef AAP_CORE_H_INCLUDED
#define AAP_CORE_H_INCLUDED

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* forward declarations */
struct AndroidAudioPluginFactory;
typedef struct AndroidAudioPluginFactory AndroidAudioPluginFactory;
struct AndroidAudioPlugin;
typedef struct AndroidAudioPlugin AndroidAudioPlugin;

typedef struct {
	size_t num_buffers;
	void **buffers;
	size_t num_frames;
} AndroidAudioPluginBuffer;

/* A minimum state support is provided in AAP framework itself.
 * LV2 has State extension, but it is provided to rather guide plugin developers
 * how to implement it. AAP is rather a bridge framework and needs simple solution.
 * Anyone can develop something similar session implementation helpers.
 */
typedef struct {
	size_t data_size;
	const void *raw_data;
} AndroidAudioPluginState;

/**
 * An entry for plugin extension instance (memory data)
 */
typedef struct {
	/** The plugin extension URI */
	const char *uri;
	/** The size of shared memory data, if needed */
	int32_t transmit_size;
	/** The optional shared memory pointer, if it wants */
	void *data;
} AndroidAudioPluginExtension;

struct AndroidAudioPluginHost;

typedef void* (*aap_host_get_exntension_t) (struct AndroidAudioPluginHost* host, const char *uri);

/**
 * Represents a host from plugin's perspective.
 * AAP client host is supposed to provide it (aap::PluginClient does so).
 * Note that it is not to represent a comprehensive host that manages multiple instances, but
 * to provide minimum information that is exposed to plugin.
 */
typedef struct AndroidAudioPluginHost {
	void *context;
	aap_host_get_exntension_t get_extension;
} AndroidAudioPluginHost;

/* function types */
typedef AndroidAudioPlugin* (*aap_instantiate_func_t) (
	AndroidAudioPluginFactory *pluginFactory,
	const char* pluginUniqueId,
	int sampleRate,
	AndroidAudioPluginHost *host);

typedef void (*aap_release_func_t) (
	AndroidAudioPluginFactory *pluginFactory,
	AndroidAudioPlugin *instance);

typedef void (*aap_prepare_func_t) (
	AndroidAudioPlugin *plugin,
	AndroidAudioPluginBuffer* audioBuffer);

typedef void (*aap_control_func_t) (AndroidAudioPlugin *plugin);

typedef void (*aap_process_func_t) (
	AndroidAudioPlugin *plugin,
	AndroidAudioPluginBuffer* audioBuffer,
	long timeoutInNanoseconds);

typedef void (*aap_get_state_func_t) (
	AndroidAudioPlugin *plugin,
	AndroidAudioPluginState *result);

typedef void (*aap_set_state_func_t) (
	AndroidAudioPlugin *plugin,
	AndroidAudioPluginState *input);

typedef void* (*aap_get_plugin_extension_func_t) (
	AndroidAudioPlugin *plugin,
	const char *extensionURI);

typedef struct AndroidAudioPlugin {
	void *plugin_specific;
	aap_prepare_func_t prepare;
	aap_control_func_t activate;
	aap_process_func_t process;
	aap_control_func_t deactivate;
	aap_get_state_func_t get_state;
	aap_set_state_func_t set_state;
	aap_get_plugin_extension_func_t get_extension;
} AndroidAudioPlugin;


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
