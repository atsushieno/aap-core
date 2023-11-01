#pragma once

#ifndef AAP_ANDROID_AUDIO_PLUGIN_H_INCLUDED
#define AAP_ANDROID_AUDIO_PLUGIN_H_INCLUDED

#include <stdint.h>
#include <stddef.h>
#include <string.h>

/* it *may* be used on some public API. Not comprehensive. */
#define AAP_PUBLIC_API __attribute__((__visibility__("default")))

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__clang__)
#define RT_SAFE [[clang::annotate("AAP_RT_SAFE")]]
#define RT_UNSAFE [[clang::annotate("AAP_RT_UNSAFE")]]
#else
#define RT_SAFE [[annotate("AAP_RT_SAFE")]]
#define RT_UNSAFE [[annotate("AAP_RT_UNSAFE")]]
#endif

#include "plugin-meta-info.h"


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
     * The pointer to the audio buffer. It may or may not be the shared pointer between host and client.
     * It may return null.*/
    void * (*get_buffer)(aap_buffer_t& self, int32_t index);

    /**
     * The default size of buffers can be calculated by num_frames * sizeof(float) or num_frames * sizeof (double),
     * but note that they can be controlled by some port parameters such as `minimumSize` property.
     * All audio ports should offer the same size.
     * `process()` must not pass `frameCount` that goes beyond the size of any AUDIO port.
     */
    int32_t (*get_buffer_size)(aap_buffer_t& self, int32_t index);
} aap_buffer_t;

/* The length of any plugin is limited to this number. */
const int32_t AAP_MAX_PLUGIN_ID_SIZE = 1024;
const int32_t AAP_MAX_EXTENSION_URI_SIZE = 1024;
/*
 * Maximum transferable AAPXS data size (either as Binder Parcelable or as MIDI2 SysEx8).
 * Anything that goes beyond this size in total should be passed via shared memory.
 */
const int32_t AAP_MAX_EXTENSION_DATA_SIZE = 1024;

/* forward declarations */
struct AndroidAudioPluginFactory;
struct AndroidAudioPlugin;

struct AndroidAudioPluginHost;

typedef void* (*aap_host_get_extension_func_t) (struct AndroidAudioPluginHost* host, const char *uri);
typedef void (*aap_host_request_process_func_t) (struct AndroidAudioPluginHost* host);

/**
 * Represents a host from plugin's perspective.
 * AAP client host is supposed to provide it (aap::PluginInstance does so).
 * Note that it is not to represent a comprehensive host that manages multiple instances, but
 * to provide minimum information that is exposed to plugin.
 *
 * The context should be capable of resolving the target plugin instance within the host so that
 * a host extension function should not have to provide `AndroidAudioPlugin` or "instance ID".
 */
typedef struct AndroidAudioPluginHost {
    void *context;
    /** returns the host extension that the host provides. */
    aap_host_get_extension_func_t get_extension;
    /** notify host that it should send process() request at inactive state. */
    aap_host_request_process_func_t request_process;
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
        int32_t frameCount,
        int64_t timeoutInNanoseconds);

typedef void* (*aap_get_extension_func_t) (
        struct AndroidAudioPlugin *plugin,
        const char *uri);

typedef aap_plugin_info_t (*get_plugin_info_func_t) (
        struct AndroidAudioPlugin* plugin,
        const char *pluginId);

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

    /** Plugin should implement this function to provide its metadata information, ports, and properties.
     * If it is not implemented, hosts might still look for plugin-info extension and query host to retrieve them instead
     * (transient behavior in the next few aap-core versions). */
    aap_plugin_info_t (*get_plugin_info) (AndroidAudioPlugin* plugin);
} AndroidAudioPlugin;

typedef void (*aapxs_completion_callback) (void* context, AndroidAudioPlugin* plugin, int32_t requestId);

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

#endif /* AAP_ANDROID_AUDIO_PLUGIN_H_INCLUDED */
