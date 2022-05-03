
#ifndef ANDROIDAUDIOPLUGIN_MIDI2_EXTENSION_H_INCLUDED
#define ANDROIDAUDIOPLUGIN_MIDI2_EXTENSION_H_INCLUDED

#include "../android-audio-plugin.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AAP_MIDI2_EXTENSION_URI "urn://androidaudioplugin.org/extensions/midi2/v1"

#define AAP_PROTOCOL_MIDI1_0 1
#define AAP_PROTOCOL_MIDI2_0 2

typedef struct AAPMidiBufferHeader {
    int32_t time_options{0};
    uint32_t length{0};
    uint32_t reserved[6];
} AAPMidiBufferHeader;

typedef struct aap_midi2_context_t {
    void *context;
    AndroidAudioPlugin* plugin;
} aap_midi2_context_t;

typedef struct aap_midi2_extension_t {
    void *context;
    int32_t protocol{0}; // 0: Unspecified / 1: MIDI1 / 2: MIDI2
    int32_t protocolVersion{0}; // MIDI1 = 0, MIDI2_V1 = 0
} aap_midi2_extension_t;

#ifdef __cplusplus
} // extern "C"
#endif

#endif // ANDROIDAUDIOPLUGIN_MIDI2_EXTENSION_H_INCLUDED
