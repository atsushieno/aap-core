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
 * A deprecated type for host extension.
 */
typedef struct {
	const char *uri;
	int32_t transmit_size;
	void *data;
} AndroidAudioPluginExtension;

/**
 * Represents a host from plugin's perspective.
 */
typedef struct AndroidAudioPluginHost {
	AndroidAudioPluginExtension** extensions;

	inline AndroidAudioPluginExtension* get_extension_entry(const char * uri) {
		for (size_t i = 0; extensions[i]; i++) {
			AndroidAudioPluginExtension *ext = extensions[i];
			if (strcmp(ext->uri, uri) == 0)
				return ext;
		}
		return NULL;
	}

	inline void* get_extension(const char * uri) {
		auto entry = get_extension_entry(uri);
		return entry ? entry->data : NULL;
	}
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
