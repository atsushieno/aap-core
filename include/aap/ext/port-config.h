#ifndef AAP_CORE_PORT_CONFIG_H
#define AAP_CORE_PORT_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../unstable/aap-core.h"
#include <stdint.h>

#define AAP_PORT_CONFIG_EXTENSION_URI "urn://androidaudioplugin.org/extensions/port-config/v1"

/*

 WARNING WARNING WARNING

 This is NOT even a draft version. More like a stub for future API definition.

 We would probably come up with strongly typed port configs with more detailed negotiation protocols.

 */

typedef struct aap_port_config {
    // it is a JSON string that is not strongly typed.
    // In this version, it should be in the form below:
    // [{"direction": "input|output", "content": "audio|midi", name: "(name)", (other arbitrary properties) }, { ... }]
    void* data;
    int32_t data_size;
} aap_port_config_t;

typedef void (*port_config_extension_get_options_t) (AndroidAudioPluginExtensionTarget target, aap_port_config_t* destination);
typedef void (*port_config_extension_select_t) (AndroidAudioPluginExtensionTarget target, int32_t configIndex);

typedef struct aap_port_config_extension_t {
    port_config_extension_get_options_t get_options;
    port_config_extension_select_t select;
} aap_port_config_extension_t;

#ifdef __cplusplus
} // extern "C"
#endif

#endif //AAP_CORE_PORT_CONFIG_H
