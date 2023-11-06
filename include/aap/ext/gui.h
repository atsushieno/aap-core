
#ifndef ANDROIDAUDIOPLUGIN_GUI_EXTENSION_H_INCLUDED
#define ANDROIDAUDIOPLUGIN_GUI_EXTENSION_H_INCLUDED

#include "../android-audio-plugin.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AAP_GUI_EXTENSION_URI "urn://androidaudioplugin.org/extensions/gui/v3"

typedef int32_t aap_gui_instance_id;

// some pre-defined error codes
#define AAP_GUI_RESULT_OK 0
#define AAP_GUI_ERROR_NO_DETAILS -1
#define AAP_GUI_ERROR_NO_GUI_DEFINED -2
#define AAP_GUI_ALREADY_INSTANTIATED -3
#define AAP_GUI_ERROR_NO_CREATE_DEFINED -10
#define AAP_GUI_ERROR_NO_SHOW_DEFINED -11
#define AAP_GUI_ERROR_NO_HIDE_DEFINED -12
#define AAP_GUI_ERROR_NO_RESIZE_DEFINED -13
#define AAP_GUI_ERROR_NO_DESTROY_DEFINED -14

// In-process GUI extension using Android View and WindowManager.
// FIXME: guiInstanceId may not be required as there should not be more than one AudioPluginView.

typedef struct aap_gui_extension_t {
    void* aapxs_context;
    // creates a new GUI View internally. It will be shown as an overlay window later (by `show()`).
    // returns > 0 for a new GUI instance ID or <0 for error code e.g. already instantiated or no GUI found.
    // Note that audioPluginView parameter is used only between AAPXS service and the plugin.
    // The plugin client process has no access to the View in the AudioPluginService process.
    // The actual instantiation could be asynchronously done.
    RT_UNSAFE aap_gui_instance_id (*create) (aap_gui_extension_t* ext, AndroidAudioPlugin* plugin, const char* pluginId, int32_t instance, void* audioPluginView);

    // shows the view (using `WindowManager.addView()`).
    // returns AAP_GUI_RESULT_OK for success, or non-zero error code. e.g. AAP_GUI_ERROR_NO_DETAILS.
    RT_UNSAFE int32_t (*show) (aap_gui_extension_t* ext, AndroidAudioPlugin* plugin, aap_gui_instance_id guiInstanceId);

    // hides the view (using `WindowManager.removeView()`).
    RT_UNSAFE void (*hide) (aap_gui_extension_t* ext, AndroidAudioPlugin* plugin, aap_gui_instance_id guiInstanceId);

    // resizes the View (by using `WindowManager.updateViewLayout()`.
    // returns AAP_GUI_RESULT_OK for success, or non-zero error code. e.g. AAP_GUI_ERROR_NO_DETAILS.
    RT_UNSAFE int32_t (*resize) (aap_gui_extension_t* ext, AndroidAudioPlugin* plugin, aap_gui_instance_id guiInstanceId, int32_t width, int32_t height);

    // frees the view.
    RT_UNSAFE void (*destroy) (aap_gui_extension_t* ext, AndroidAudioPlugin* plugin, aap_gui_instance_id guiInstanceId);
} aap_gui_extension_t;

#ifdef __cplusplus
} // extern "C"
#endif

#endif // ANDROIDAUDIOPLUGIN_GUI_EXTENSION_H_INCLUDED
