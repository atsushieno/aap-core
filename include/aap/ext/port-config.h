#ifndef AAP_CORE_PORT_CONFIG_H
#define AAP_CORE_PORT_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../android-audio-plugin.h"
#include <stdint.h>

#define AAP_PORT_CONFIG_EXTENSION_URI "urn://androidaudioplugin.org/extensions/port-config/v2"

#define AAP_PORT_CONFIG_DEFAULT_INSTRUMENT "urn://androidaudioplugin.org/port-config/default/instrument/v1"
#define AAP_PORT_CONFIG_DEFAULT_EFFECT "urn://androidaudioplugin.org/port-config/default/effect/v1"

/*

 WARNING WARNING WARNING

 This is NOT even a draft version. More like a stub for future API definition.

 We would probably come up with strongly typed port configs with more detailed negotiation protocols.

  ----

  With port-config extension existing in aap_metadata.xml, a host that supports it will automatically set up
  ports unless an explicit <ports> declaration exists. For automatic setup, it is in this order:
  - MIDI IN (used to receiver parameter changes)
  - MIDI OUT (used to send parameter change notification via edit controller  / UI)
  - for Effect plugins, AUDIO IN
  - AUDIO OUT

  The layout result is sent at confirm request.
  FIXME: change the API structure for that.

 */

typedef struct aap_port_config {
    // For request from host, indicates nothing (it cannot "require" a plugin to have a MIDI input).
    // For reply from plugin, indicates that it expects MIDI inputs.
    bool has_midi_input;
    // For request from host, indicates nothing (it cannot "require" a plugin to have a MIDI output).
    // For reply from plugin, indicates that it emits MIDI outputs.
    bool has_midi_output;
    // The number of generic unconditional audio ports without any named channel settings.
    // It would be useful for effect plugins which does not care about their output position.
    // For request from host, indicates the number of audio ports the host will connect.
    // For reply from plugin, 0 means the requested number is not supported, and the same number kept means it is supported. Other values are reserved (invalid).
    int32_t num_audio_ports;
    // A named port configuration setup, reserved for future uses  (e.g. for Ambisonic) in the future versions of this extension or other extensions.
    // For request from host, indicates that the host will use the named config.
    // For reply from plugin, NULL means it is not supported by the plugin, and the same pointer kept means it is supported. Other values are reserved (invalid).
    const char* named_config;
    // it is a null-terminated JSON string that is not strongly typed. Reserved for future use.
    void* extra_data;
    int32_t extra_data_size;
} aap_port_config_t;

typedef struct aap_port_config_extension_t {
    void* aapxs_context;
    // It is supposed to be stored/cached in memory
    RT_SAFE void (*get_options) (aap_port_config_extension_t* ext, AndroidAudioPlugin* plugin, aap_port_config_t* destination);
    // It is supposed to be stored/cached in memory
    RT_SAFE void (*select) (aap_port_config_extension_t* ext, AndroidAudioPlugin* plugin, const char* configuration);
} aap_port_config_extension_t;

#ifdef __cplusplus
} // extern "C"
#endif

#endif //AAP_CORE_PORT_CONFIG_H
