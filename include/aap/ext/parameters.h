
#ifndef ANDROIDAUDIOPLUGIN_PARAMETERS_EXTENSION_H_INCLUDED
#define ANDROIDAUDIOPLUGIN_PARAMETERS_EXTENSION_H_INCLUDED

#include "../android-audio-plugin.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AAP_PARAMETERS_EXTENSION_URI "urn://androidaudioplugin.org/extensions/parameters/v2.1"
#define AAP_PARAMETERS_XMLNS_URI "urn://androidaudioplugin.org/extensions/parameters"

#define AAP_MAX_PARAMETER_NAME_CHARS 256
#define AAP_MAX_PARAMETER_PATH_CHARS 256
#define AAP_MAX_PARAMETER_ENUM_NAME 80

#define AAP_PARAMETER_QUANTIZED_TYPE_BOOLEAN 2
#define AAP_PARAMETER_QUANTIZED_TYPE_INTEGER 3

// These might not be returned as a property. These constants are used in Kotlin though.
#define AAP_PARAMETER_PROPERTY_MIN_VALUE 1
#define AAP_PARAMETER_PROPERTY_MAX_VALUE 2
#define AAP_PARAMETER_PROPERTY_DEFAULT_VALUE 3
// It helps to determine which parameters should be displayed on generic compact views.
// It does not imply anything other than priority (e.g. "-1 means never show up"; it is up to host)
#define AAP_PARAMETER_PROPERTY_PRIORITY 11
// It indicates that the returned value may be still valid if it is not one of the enumerated values.
#define AAP_PARAMETER_PROPERTY_IS_DISCRETE 12
// An optional type identifier e.g. `AAP_PARAMETER_QUANTIZED_TYPE_BOOLEAN` or `AAP_PARAMETER_QUANTIZED_TYPE_INTEGER`
// Host UI may change the input control types depending on this.
#define AAP_PARAMETER_PROPERTY_QUANTIZED_TYPE 13

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

typedef struct aap_parameter_enum_t {
    char name[AAP_MAX_PARAMETER_ENUM_NAME];
    double value;
} aap_parameter_enum_t;

typedef struct aap_parameters_extension_t {
    void* aapxs_context;
    // Returns the number of parameters.
    // If the plugin does not provide the parameter list on aap_metadata, it is supposed to provide them here.
    // On the other hand, if the code wants to tell that the code is not going to provide the parameter list, it can return -1 (explicitly).
    RT_SAFE int32_t (*get_parameter_count) (aap_parameters_extension_t* ext, AndroidAudioPlugin *plugin);
    // Returns the parameter information by parameter index (NOT by ID).
    // If the plugin does not provide the parameter list on aap_metadata, it is supposed to provide them here.
    RT_UNSAFE aap_parameter_info_t (*get_parameter) (aap_parameters_extension_t* ext, AndroidAudioPlugin *plugin, int32_t index);

    // Returns the parameter property in a double (64-bit) value, by parameter ID and property ID.
    // It should return `0` if the requested property does not exist.
    RT_UNSAFE double (*get_parameter_property) (aap_parameters_extension_t* ext, AndroidAudioPlugin *plugin, int32_t parameterId, int32_t propertyId);

    // Returns the number of enumerated values for a parameter, by parameter ID.
    // It should return `0` if it is not an enumerated parameter.
    RT_UNSAFE int32_t (*get_enumeration_count) (aap_parameters_extension_t* ext, AndroidAudioPlugin *plugin, int32_t parameterId);
    // Returns the value of an enumeration for a parameter, by parameter ID and enum index.
    // It should return NULL if it is not an enumerated parameter.
    RT_UNSAFE aap_parameter_enum_t (*get_enumeration) (aap_parameters_extension_t* ext, AndroidAudioPlugin *plugin, int32_t parameterId, int32_t enumIndex);
} aap_parameters_extension_t;

typedef struct aap_host_parameters_extension_t {
    // Notifies host that parameter layout is being changed.
    // THe actual parameter list needs to be queried by host (it will need to refresh the list anyways).
    // FIXME: the `host` argument could be replaced with ext->plugin_context
    // FIXME: the `plugin` argument could be represented by something else e.g. instanceID.
    RT_SAFE void (*notify_parameters_changed) (aap_host_parameters_extension_t* ext, AndroidAudioPluginHost* host, AndroidAudioPlugin *plugin);
} aap_host_parameters_extension_t;

#ifdef __cplusplus
} // extern "C"
#endif

#endif // ANDROIDAUDIOPLUGIN_PARAMETERS_EXTENSION_H_INCLUDED
