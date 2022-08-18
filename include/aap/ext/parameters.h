
#ifndef ANDROIDAUDIOPLUGIN_PARAMETERS_EXTENSION_H_INCLUDED
#define ANDROIDAUDIOPLUGIN_PARAMETERS_EXTENSION_H_INCLUDED

#include "../android-audio-plugin.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AAP_PARAMETERS_EXTENSION_URI "urn://androidaudioplugin.org/extensions/parameters/v1"
#define AAP_PARAMETERS_XMLNS_URI "urn://androidaudioplugin.org/extensions/parameters"

#define AAP_MAX_PARAMETER_NAME_CHARS 256
#define AAP_MAX_PARAMETER_PATH_CHARS 256

typedef struct aap_parameter_info_t {
    // Parameter ID.
    // For plugin users sake, parameter ID must be "stable", preserved for backward compatibility
    // (otherwise songs your users authored get broken and you plugin developers will be blamed).
    // Since there may be "dropped" indices, this does not mean the index in any parameter list.
    //
    // Type is int32_t so that any API that deals with parameters can return -1 for "not found" etc.
    int32_t stable_id;
    char display_name[AAP_MAX_PARAMETER_NAME_CHARS];
    char path[AAP_MAX_PARAMETER_PATH_CHARS];
    double min_value;
    double max_value;
    double default_value;
} aap_parameter_info_t;

typedef int32_t (*presets_extension_get_parameter_count_func_t) (AndroidAudioPluginExtensionTarget target);
typedef aap_parameter_info_t* (*presets_extension_get_parameter_func_t) (AndroidAudioPluginExtensionTarget target, int32_t index);

typedef struct aap_parameters_extension_t {
    presets_extension_get_parameter_count_func_t get_parameter_count;
    presets_extension_get_parameter_func_t get_parameter;
} aap_parameters_extension_t;

#ifdef __cplusplus
} // extern "C"
#endif

#endif // ANDROIDAUDIOPLUGIN_PARAMETERS_EXTENSION_H_INCLUDED
