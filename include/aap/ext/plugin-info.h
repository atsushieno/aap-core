#ifndef ANDROIDAUDIOPLUGIN_PLUGIN_INFO_EXTENSION_H_INCLUDED
#define ANDROIDAUDIOPLUGIN_PLUGIN_INFO_EXTENSION_H_INCLUDED

// plugin-info extension is used primarily by a plugin to retrieve its own metadata, ports, and
// parameters from the host that had parsed `aap_metadata.xml` and configured ports etc.
// Those information items are not obviously known to plugin wrappers, including our own local host.
//
// IPC is not involved for `get()`, as it is implemented at LocalPluginInstance. A local plugin host should be
// able to resolve all the plugin information items.
//
// It is prohibited for `notify_plugin_info_update()` to make changes to `plugin_package_name`,
// `plugin_local_name`, `identifier_string` and `plugin_id`.
// Host should treat such changes as an error and invalidate the plugin.
// FIXME: rename `notify_plugin_info_update` to `notify_update`.

#ifdef __cplusplus
extern "C" {
#endif

#include "../android-audio-plugin.h"
#include "stdint.h"

#define AAP_PLUGIN_INFO_EXTENSION_URI "urn://androidaudioplugin.org/extensions/plugin-info/v2"

enum aap_content_type {
    AAP_CONTENT_TYPE_UNDEFINED = 0,
    AAP_CONTENT_TYPE_AUDIO = 1,
    AAP_CONTENT_TYPE_MIDI = 2,
    // FIXME: we will remove it. There will be only one MIDI port that would switch
    //  between MIDI2 and MIDI1, using MIDI-CI Set New Protocol.
    AAP_CONTENT_TYPE_MIDI2 = 3,
};

enum aap_port_direction {
    AAP_PORT_DIRECTION_INPUT,
    AAP_PORT_DIRECTION_OUTPUT
};

typedef struct aap_plugin_info_port aap_plugin_info_port_t;

typedef aap_port_direction (*get_port_direction_property_func)(aap_plugin_info_port_t* port);
typedef aap_content_type (*get_content_type_property_func)(aap_plugin_info_port_t* port);
typedef uint32_t (*get_uint32_port_property_func)(aap_plugin_info_port_t* port);
typedef const char* (*get_string_port_property_func)(aap_plugin_info_port_t* port);

struct aap_plugin_info_port {
    void *context;
    get_uint32_port_property_func index;
    get_string_port_property_func name;
    get_content_type_property_func content_type;
    get_port_direction_property_func direction;
};

typedef struct aap_plugin_info_parameter aap_plugin_info_parameter_t;

typedef uint32_t (*get_uint32_parameter_property_func)(aap_plugin_info_parameter_t* parameter);
typedef float (*get_float_parameter_property_func)(aap_plugin_info_parameter_t* parameter);
typedef const char* (*get_string_parameter_property_func)(aap_plugin_info_parameter_t* parameter);

struct aap_plugin_info_parameter {
    RT_SAFE void *context;
    RT_SAFE get_uint32_parameter_property_func id;
    RT_UNSAFE get_string_parameter_property_func name;
    RT_SAFE get_float_parameter_property_func min_value;
    RT_SAFE get_float_parameter_property_func max_value;
    RT_SAFE get_float_parameter_property_func default_value;
};

typedef struct aap_plugin_info_t aap_plugin_info_t;

typedef uint32_t (*get_uint32_plugin_property_func)(aap_plugin_info_t* plugin);
typedef const char* (*get_string_plugin_property_func)(aap_plugin_info_t* plugin);
typedef aap_plugin_info_parameter_t (*get_plugin_info_get_parameter_func)(aap_plugin_info_t* plugin, uint32_t port);
typedef aap_plugin_info_port_t (*get_plugin_info_get_port_func)(aap_plugin_info_t* plugin, uint32_t port);

struct aap_plugin_info_t {
    void *context;
    RT_UNSAFE get_string_plugin_property_func plugin_package_name;
    RT_UNSAFE get_string_plugin_property_func plugin_local_name;
    RT_UNSAFE get_string_plugin_property_func display_name;
    RT_UNSAFE get_string_plugin_property_func developer_name;
    RT_UNSAFE get_string_plugin_property_func version;
    RT_UNSAFE get_string_plugin_property_func primary_category;
    RT_UNSAFE get_string_plugin_property_func identifier_string;
    RT_UNSAFE get_string_plugin_property_func plugin_id;

    RT_SAFE get_uint32_plugin_property_func get_port_count;
    RT_SAFE get_plugin_info_get_port_func get_port;
    RT_SAFE get_uint32_plugin_property_func get_parameter_count;
    RT_SAFE get_plugin_info_get_parameter_func get_parameter;
};

typedef struct aap_plugin_info_extension_t {
    RT_SAFE void (*notify_plugin_info_update) (aap_plugin_info_extension_t* ext);
} aap_plugin_info_extension_t;

typedef struct aap_host_plugin_info_extension_t {
    RT_UNSAFE aap_plugin_info_t (*get) (aap_host_plugin_info_extension_t* ext, AndroidAudioPluginHost* host, const char *pluginId);
} aap_host_plugin_info_extension_t;

#ifdef __cplusplus
}
#endif

#endif // ANDROIDAUDIOPLUGIN_PLUGIN_INFO_EXTENSION_H_INCLUDED
