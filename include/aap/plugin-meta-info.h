#ifndef ANDROIDAUDIOPLUGIN_PLUGIN_META_INFO_H_INCLUDED
#define ANDROIDAUDIOPLUGIN_PLUGIN_META_INFO_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

#include "stdint.h"

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
    void *context;
    get_uint32_parameter_property_func id;
    get_string_parameter_property_func name;
    get_float_parameter_property_func min_value;
    get_float_parameter_property_func max_value;
    get_float_parameter_property_func default_value;
};

typedef struct aap_plugin_info_t aap_plugin_info_t;

typedef uint32_t (*get_uint32_plugin_property_func)(aap_plugin_info_t* plugin);
typedef const char* (*get_string_plugin_property_func)(aap_plugin_info_t* plugin);
typedef aap_plugin_info_parameter_t (*get_plugin_info_get_parameter_func)(aap_plugin_info_t* plugin, uint32_t port);
typedef aap_plugin_info_port_t (*get_plugin_info_get_port_func)(aap_plugin_info_t* plugin, uint32_t port);

typedef struct aap_plugin_info_t {
    void *context;
    get_string_plugin_property_func plugin_package_name;
    get_string_plugin_property_func plugin_local_name;
    get_string_plugin_property_func display_name;
    get_string_plugin_property_func developer_name;
    get_string_plugin_property_func version;
    get_string_plugin_property_func primary_category;
    get_string_plugin_property_func identifier_string;
    get_string_plugin_property_func plugin_id;

    get_uint32_plugin_property_func get_port_count;
    get_plugin_info_get_port_func get_port;
    get_uint32_plugin_property_func get_parameter_count;
    get_plugin_info_get_parameter_func get_parameter;
} aap_plugin_info_t;

#ifdef __cplusplus
}
#endif

#endif // ANDROIDAUDIOPLUGIN_PLUGIN_META_INFO_H_INCLUDED
