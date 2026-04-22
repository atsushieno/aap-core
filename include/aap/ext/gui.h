
#ifndef ANDROIDAUDIOPLUGIN_GUI_EXTENSION_H_INCLUDED
#define ANDROIDAUDIOPLUGIN_GUI_EXTENSION_H_INCLUDED

#include "../android-audio-plugin.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AAP_GUI_EXTENSION_URI "urn://androidaudioplugin.org/extensions/gui/v4"

typedef int32_t aap_gui_instance_id;

// Some predefined result/error codes for the GUI extension callbacks.
#define AAP_GUI_RESULT_OK 0
#define AAP_GUI_ERROR_NO_DETAILS -1
#define AAP_GUI_ERROR_NO_GUI_DEFINED -2
#define AAP_GUI_ALREADY_INSTANTIATED -3
#define AAP_GUI_ERROR_NO_CREATE_DEFINED -10
#define AAP_GUI_ERROR_NO_SHOW_DEFINED -11
#define AAP_GUI_ERROR_NO_HIDE_DEFINED -12
#define AAP_GUI_ERROR_NO_RESIZE_DEFINED -13
#define AAP_GUI_ERROR_NO_DESTROY_DEFINED -14

// Generic GUI lifecycle extension for AAP plugins.
//
// This header exposes the C-level extension surface that native/JNI code can see.
// It should not be read as the authoritative description of the current Android
// embedded-view hosting implementation.
//
// In current AAP code, the modern embedded native View path is primarily driven
// by Kotlin/Android-side components such as AudioPluginViewService and
// AudioPluginSurfaceControlClient. Those components use their own internal
// Messenger protocol and their own session identifiers.
//
// Therefore:
// - aap_gui_instance_id identifies an instance managed by this extension layer.
// - It is not the same thing as the guiSessionId used by AudioPluginViewService.
// - The exact meaning of create/show/hide/resize/destroy is implementation
//   dependent beyond the basic lifecycle intent documented below.
// - Callbacks are RT_UNSAFE and must not be invoked from the audio thread.

typedef struct aap_gui_extension_t {
    void* aapxs_context;

    // Creates or connects a GUI instance for the given plugin instance.
    //
    // Returns a positive extension-layer GUI instance ID on success, or a
    // negative error code on failure.
    //
    // audioPluginView is an implementation-defined opaque handle used by some
    // host/service bridges. It must not be assumed to be a directly accessible
    // Android View across processes.
    //
    // Implementations may complete the actual UI setup asynchronously.
    RT_UNSAFE aap_gui_instance_id (*create) (aap_gui_extension_t* ext, AndroidAudioPlugin* plugin, const char* pluginId, int32_t instance, void* audioPluginView);

    // Requests that a previously created GUI instance become visible/active.
    //
    // Returns AAP_GUI_RESULT_OK on success or a negative error code on failure.
    RT_UNSAFE int32_t (*show) (aap_gui_extension_t* ext, AndroidAudioPlugin* plugin, aap_gui_instance_id guiInstanceId);

    // Requests that a GUI instance become hidden/inactive while remaining alive.
    RT_UNSAFE void (*hide) (aap_gui_extension_t* ext, AndroidAudioPlugin* plugin, aap_gui_instance_id guiInstanceId);

    // Requests a new GUI size in implementation-defined units.
    //
    // For the current Android embedded-view path, higher-level viewport/content
    // negotiation is handled outside this C header. Callers must not assume that
    // this callback alone fully describes current SurfaceControlViewHost-based UI
    // behavior.
    //
    // Returns AAP_GUI_RESULT_OK on success or a negative error code on failure.
    RT_UNSAFE int32_t (*resize) (aap_gui_extension_t* ext, AndroidAudioPlugin* plugin, aap_gui_instance_id guiInstanceId, int32_t width, int32_t height);

    // Destroys a GUI instance and releases resources associated with it.
    //
    // After destroy(), guiInstanceId is no longer valid.
    RT_UNSAFE void (*destroy) (aap_gui_extension_t* ext, AndroidAudioPlugin* plugin, aap_gui_instance_id guiInstanceId);
} aap_gui_extension_t;

#ifdef __cplusplus
} // extern "C"
#endif

#endif // ANDROIDAUDIOPLUGIN_GUI_EXTENSION_H_INCLUDED
