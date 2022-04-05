
#if !ANDROIDAUDIOPLUGIN_MIDI2_EXTENSION_H_INCLUDED
#define ANDROIDAUDIOPLUGIN_MIDI2_EXTENSION_H_INCLUDED 1

#ifdef __cplusplus
extern "C" {
#endif

#define AAP_MIDI_CI_EXTENSION_URI "urn://androidaudioplugin.org/extensions/midi-ci/v1"

#define AAP_PROTOCOL_MIDI1_0 1
#define AAP_PROTOCOL_MIDI2_0 2

typedef struct {
    int32_t protocol{0}; // 0: Unspecified / 1: MIDI1 / 2: MIDI2
    int32_t protocolVersion{0}; // MIDI1 = 0, MIDI2_V1 = 0
} MidiCIExtension;

typedef struct {
    int32_t time_options{0};
    uint32_t length{0};
    uint32_t reserved[6];
} AAPMidiBufferHeader;

#ifdef __cplusplus
} // extern "C"
#endif

#endif // ANDROIDAUDIOPLUGIN_MIDI2_EXTENSION_H_INCLUDED
