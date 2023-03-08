
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
    // Since there may be "dropped" parameters after continuous development, the parameter ID
    // does not mean the index in any parameter list.
    //
    // Type is int16_t and the expected parameter range is actually 0 to 16383 so that...
    // - any API that deals with parameters can return -1 for "not found" etc.
    // - it can be mapped to MIDI 1.0 7-bit MSB and LSB, which is compatible with NRPN or
    //   MIDI 2.0 Assignable Controllers.
    int16_t stable_id;
    // A name string to display, for human beings etc.
    char display_name[AAP_MAX_PARAMETER_NAME_CHARS];
    // A file-tree style structural path for the parameter (following the CLAP way here).
    // Applications would split it by '/', ignoring empty entries e.g.:
    //   `<parameter id="0" name="OSC1 Reverb Send Level", path="/OSC1/Reverb" />`
    //   `<parameter id="1" name="OSC1 Reverb Send Depth", path="/OSC1/Reverb" />`
    //   `<parameter id="2" name="OSC1 Delay Send Level", path="/OSC1/Delay" />`
    //   ...
    //  - OSC1
    //    - Reverb
    //      - OSC1 Reverb Send Level
    //      - OSC1 Reverb Send Depth
    //    - Delay
    //      - OSC1 Delay Send Level
    //    ...
    char path[AAP_MAX_PARAMETER_PATH_CHARS];
    // the actual value range would be of float, but in case we support double in the future it will ont lose API compatibility here.
    double min_value;
    double max_value;
    double default_value;
    bool per_note_enabled;
} aap_parameter_info_t;

typedef int32_t (*parameters_extension_get_parameter_count_func_t) (AndroidAudioPluginExtensionTarget target);
typedef aap_parameter_info_t* (*parameters_extension_get_parameter_func_t) (AndroidAudioPluginExtensionTarget target, int32_t index);

typedef struct aap_parameters_extension_t {
    // Returns the number of parameters.
    // If the plugin does not provide the parameter list on aap_metadata, it is supposed to provide them here.
    RT_SAFE parameters_extension_get_parameter_count_func_t get_parameter_count;
    // Returns the parameter information by parameter index (NOT by ID).
    // If the plugin does not provide the parameter list on aap_metadata, it is supposed to provide them here.
    RT_SAFE parameters_extension_get_parameter_func_t get_parameter;
} aap_parameters_extension_t;

typedef void (*parameters_host_extension_notify_parameter_list_changed_func_t) (AndroidAudioPluginHost* host, AndroidAudioPlugin *plugin);

typedef struct aap_host_parameters_extension_t {
    // Notifies host that parameter layout is being changed.
    // THe actual parameter list needs to be queried by host (it will need to refresh the list anyways).
    RT_SAFE parameters_host_extension_notify_parameter_list_changed_func_t notify_parameters_changed;
} aap_host_parameters_extension_t;

#ifdef __cplusplus
} // extern "C"
#endif

#endif // ANDROIDAUDIOPLUGIN_PARAMETERS_EXTENSION_H_INCLUDED
