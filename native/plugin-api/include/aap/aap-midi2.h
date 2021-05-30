
#if !ANDROIDAUDIOPLUGIN_MIDI2_EXTENSION_H_INCLUDED
#define ANDROIDAUDIOPLUGIN_MIDI2_EXTENSION_H_INCLUDED 1
namespace aap {

#define AAP_MIDI_CI_EXTENSION_URI "urn://androidaudioplugin.org/extensions/midi-ci/v1"

class MidiCIExtension {
public:
    int32_t protocol{0}; // 0: Unspecified / 1: MIDI1 / 2: MIDI2
    int32_t protocolVersion{0}; // MIDI1 = 0, MIDI2_V1 = 0
};


} // namespace aap

#endif // ANDROIDAUDIOPLUGIN_MIDI2_EXTENSION_H_INCLUDED
