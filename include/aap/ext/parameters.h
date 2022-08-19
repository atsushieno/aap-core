
#ifndef ANDROIDAUDIOPLUGIN_PARAMETERS_EXTENSION_H_INCLUDED
#define ANDROIDAUDIOPLUGIN_PARAMETERS_EXTENSION_H_INCLUDED

#include "../android-audio-plugin.h"

#ifdef __cplusplus
extern "C" {
#endif

// This parameters extension assumes that parameter updates by host are sent to the plugin using
// MIDI2 input port, and parameter change notification back to the host is sent from the plugin
// using MIDI2 output port.
//
// The way how parameter changes and notifications are transmitted depends on get_mapping_policy().
// extension function.
// - When it returns `AAP_PARAMETERS_MAPPING_POLICY_ACC`, then the host can assume that parameter
//   changes requests from the host and the change notification to the host can be represented as
//   MIDI2 Assignable Controller (ACC) UMP.
// - When it returns `AAP_PARAMETERS_MAPPING_POLICY_ACC2`, it works quite like
//   AAP_PARAMETERS_MAPPING_POLICY_ACC, except that for AAP parameter changes and notifications
//   the most significant BIT of the ACC MSB is always 1, so that the range of the valid parameter
//   index is limited (but almost harmless in practive - who has >32768 parameters?).
//   For this mode, plugins are responsible to remove tha flag when decoding the ACC UMP message.
//   It is useful IF the plugin should receive any MIDI message sent by host.
// - None does not has any premise. Host in general has no idea how to send the parameter changes.
//   Some kind of special hosts may be able to control such a plugin (for example, via some
//   Universal System Exclusive messages). Or the plugin might want to
//   expose all those parameters but not control and/or notify change publicly (all at once?).
//
// The Assignable controller index is limited to 31-bit, and the mst significant *bit* of the MSB
// is always 1 for AAP parameter change message. For any other Assignable Controller (etc.) messages.
// it is up to host and plugin. (We don't want to "map" and therefore harm MIDI inputs like VST3 does.)
// Supporting parameters up to 32768 would be more than reasonable.
//
// They can also be Relative Assignable Controoller, or Assignable Per-Note Controller, but it is
// up to the plugin if it accepts them or not.

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
    // A name string to display, for human beings etc.
    char display_name[AAP_MAX_PARAMETER_NAME_CHARS];
    // A file-tree style structural path for the parameter.
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
} aap_parameter_info_t;

enum aap_parameters_mapping_policy {
    AAP_PARAMETERS_MAPPING_POLICY_NONE = 0,
    AAP_PARAMETERS_MAPPING_POLICY_ACC = 1,
    AAP_PARAMETERS_MAPPING_POLICY_ACC2 = 2
};

typedef int32_t (*parameters_extension_get_parameter_count_func_t) (AndroidAudioPluginExtensionTarget target);
typedef aap_parameter_info_t* (*parameters_extension_get_parameter_func_t) (AndroidAudioPluginExtensionTarget target, int32_t index);
typedef enum aap_parameters_mapping_policy (*parameters_extension_get_mapping_policy_func_t) (AndroidAudioPluginExtensionTarget target);

typedef struct aap_parameters_extension_t {
    parameters_extension_get_parameter_count_func_t get_parameter_count;
    parameters_extension_get_parameter_func_t get_parameter;
    parameters_extension_get_mapping_policy_func_t get_mapping_policy;
} aap_parameters_extension_t;

#ifdef __cplusplus
} // extern "C"
#endif

#endif // ANDROIDAUDIOPLUGIN_PARAMETERS_EXTENSION_H_INCLUDED
