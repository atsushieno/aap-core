
#ifndef ANDROIDAUDIOPLUGIN_GUI_EXTENSION_H_INCLUDED
#define ANDROIDAUDIOPLUGIN_GUI_EXTENSION_H_INCLUDED

#include "../android-audio-plugin.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AAP_GUI_EXTENSION_URI "urn://androidaudioplugin.org/extensions/gui/v1"

#define AAP_GUI_RESULT_OK 0
#define AAP_GUI_RESULT_UNSPECIFIED_ERROR 1

// it should return AAP_GUI_RESULT_OK for success.
// Non-zero return value will be defined as error code (TODO: define them if needed).
// So far AAP_GUI_RESULT_UNSPECIFIED_ERROR is available.
typedef int32_t (*gui_extension_show_func_t) (AndroidAudioPluginExtensionTarget target);

typedef void (*gui_extension_hide_func_t) (AndroidAudioPluginExtensionTarget target);

typedef struct aap_gui_extension_t {
    gui_extension_show_func_t show;
    gui_extension_hide_func_t hide;
} aap_gui_extension_t;

#ifdef __cplusplus
} // extern "C"
#endif

#endif // ANDROIDAUDIOPLUGIN_GUI_EXTENSION_H_INCLUDED
