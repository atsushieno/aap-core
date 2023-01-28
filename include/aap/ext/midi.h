
#ifndef ANDROIDAUDIOPLUGIN_MIDI_EXTENSION_H_INCLUDED
#define ANDROIDAUDIOPLUGIN_MIDI_EXTENSION_H_INCLUDED

#include "../android-audio-plugin.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AAP_MIDI_EXTENSION_URI "urn://androidaudioplugin.org/extensions/midi2/v1"

#define AAP_PROTOCOL_MIDI1_0 1
#define AAP_PROTOCOL_MIDI2_0 2

typedef struct AAPMidiBufferHeader {
    int32_t time_options{0};
    uint32_t length{0};
    uint32_t reserved[6];
} AAPMidiBufferHeader;

//
// This MIDI extension assumes that parameter updates by host are sent to the plugin using
// MIDI2 inputs, and parameter change notification back to the host is sent from the plugin
// using MIDI2 outputs. The source input is a MIDI2 input port, and the destination
// should be a MIDI2 output port. There is the only one input and the only one output.
//
// ## MIDI2-parameters mapping schemes
//
// The way how parameter changes and notifications are transmitted depends on get_mapping_policy().
// extension function. It returns a set of flags where -
// - `AAP_PARAMETERS_MAPPING_POLICY_CC` indicates that the plugin will consume Control Change inputs
//   by its own and cannot be used for parameter changes.
// - `AAP_PARAMETERS_MAPPING_POLICY_ACC` indicates that the plugin will consume Assignable Controller
//   and Per-Note Assignable Controller inputs by its own and cannot be used for parameter changes.
// - `AAP_PARAMETERS_MAPPING_POLICY_PROGRAM` indicates that the plugin will consume Program Change
//   inputs (and bank select to represent 8+ bits i.e. preset = bank * 128 + program) by its own
//   and cannot be used for preset setter.
// - `AAP_PARAMETERS_MAPPING_POLICY_SYSEX8` indicates that the plugin will consume such a Universal
//   SysEx8 message that conforms to certain SysEx8 packet by its own and cannot be used for
//   parameter changes. It should be almost always disabled. Hosts may not allow it.
//   And if it is enabled, then at least one of ..._CC or ..._ACC flag should be disabled.
//
// If a plugin does not want to let host send CC/ACC/Program (or SysEx8) for alternative purposes,
// then it should implement this extension to return its own mapping policy.
//
// AAP MidiDeviceService will translate those CC and ACC messages to parameter change SysEx8, and
// translate Program Changes to preset changes, unless the plugin requires them to be passed
// and not injected.
//
// The sysex8 as parameter change UMP consists of a single UMP packet (i.e. 16 bits) and
// looks like `5g sz pc 7E  7F 00 00 ch  k.n.idx.  value...`, where -
//   - g : UMP group
//   - sz : status and size, always 0F
//   - pc : packet, always 00
//   - ch : channel, 00-0F
//   - k. : key for per-note parameter change, 00-7F
//   - n. : reserved, but it might be used for note ID for per note parameter change, 00-7F.
//          If it is being used for note ID, host can assign a consistent number across note on/off
//          w/ attribute and this message. But I find it ugly and impractical so far.
//   - idx. : parameter index, 0-16383
//   - value... : parameter value, 32-bit float
//
// ## UI events
//
// At this state, we decided to not provide its own event queuing entrypoint in this extension.
// Host is responsible to send remote-UI-originated events within its `process()` calls.
//
// It is easier to ask every host developer to implement event input unification i.e. unify the UI
// event inputs into its MIDI2 inputs for `process()`, than asking every plugin developer to do it.
// AAP has two kinds of UIs: local in-plugin-process UI and remote in-host-process UI:
//
// - For remote UI, the UI is a Web UI which dispatches the input events to the host WebView using
//   JavaScript interface, and there is (will be) the event dispatching API for that, as well as
//   parameter change notification listener API. Then in terms of host-plugin interaction, it is
//   totally a matter of `process()`
//
// - For local UI, it is totally within the plugin process, thus it is up to the plugin itself
//   to implement how to interpret its own UI events to the MIDI2 input queue.
//
// In either way, host will receive parameter change notifications through the MIDI2 outputs,
// synchronously. For remote UI, the host will have to dispatch the change notifications to the
// Web UI (via the parameter change notification listener API).
//
// This decision is actually tentative and we may introduce additional event queuing function
// that would enhance use of parameters.
//
// ### Some technical background on parameters and UI
//
// In the world of audio plugin formats, there are two kinds of parameter support "extensions" :
//
// - LV2 has strict distinction between DSP and UI, which results in detached dynamic library
//   entity (dll/dylib/so) for UI from DSP, and it needs certain interface (API) for those two.
//   Since it is for UI, it is not a synchronous API. It writes events to the port, without
//   reading any "response" notifications. They will be sent to output ports at some stage.
// - CLAP has parameters extension in totally different way. It has `flush()` that host can tell
//   plugin to process parameters and get parameter change notifications synchronously.
//   It does not care about UI/DSP separation. It only cares the interface between host and plugin.
//
// <del>
// The in-process UI and any other out-process UIs, apart from the primary sequencer (DAW) MIDI2
// inputs, should also be able to send parameter changes. That is why we will have `addEvents()`
// member function as part of this extension.
// Note that it is not usable for "receiving" change notifications, as change notifications
// would be often sent as to notify the "latest value" which cannot be really calculated without
// the actual audio processing inputs.
// To not interrupt realtime audio processing loop, additional events from UI should be enqueued,
// not processed within the call to the processing function. We could name the function as
// `process()` instead of `addEvents()` and specify outputs stream as an argument too, but that
// implies we generate the outputs *on time*, which is not realistic (the function should behave
// like "enqueue and return immediately", nothing to process further to get outputs).
// </del>
//

// keep these in sync with AudioPluginMidiSettings.
enum aap_midi_mapping_policy {
    AAP_PARAMETERS_MAPPING_POLICY_NONE = 0,
    AAP_PARAMETERS_MAPPING_POLICY_ACC = 1,
    AAP_PARAMETERS_MAPPING_POLICY_CC = 2,
    AAP_PARAMETERS_MAPPING_POLICY_SYSEX8 = 4,
    AAP_PARAMETERS_MAPPING_POLICY_PROGRAM = 8,
};

static inline void aapMidi2ParameterSysex8(uint32_t *dst1, uint32_t *dst2, uint32_t *dst3, uint32_t *dst4, uint8_t group, uint8_t channel, uint8_t key, uint16_t extra, uint16_t index, float value) {
    *dst1 = ((0x50 + group) << 24) + 0x7E;
    *dst2 = 0x7F000000 + channel;
    *dst3 = (key << 24) + (extra << 16) + index;
    *dst4 = *(uint32_t*) (void*) &value;
}

static inline bool aapReadMidi2ParameterSysex8(uint8_t* group, uint8_t* channel,
                                               uint8_t* key, uint8_t* extra, uint16_t* index, float* value,
                                               uint32_t src1, uint32_t src2, uint32_t src3, uint32_t src4) {
    *group = (uint8_t) (src1 >> 24) & 0xF;
    *channel = src2 & 0xF;
    *key = src3 >> 24;
    *extra = (src3 >> 16) & 0xFF;
    *index = src3 & 0xFFFF;
    *value = *(float*) &src4;
    return (src1 >> 24) == 0x50 + *group && (src1 & 0xFF) == 0x7E && src2 == 0x7F000000 + *channel;
}

typedef enum aap_midi_mapping_policy (*midi_extension_get_mapping_policy_func_t) (AndroidAudioPluginExtensionTarget target, const char* pluginId);

// The MIDI extension

typedef struct aap_midi_extension_t {
    // indicates which UMP protocol it expects for the inputs: 0: Unspecified (MIDI2) / 1: MIDI1 / 2: MIDI2
    // Currently unused.
    int32_t protocol{0};
    // indicates the expected UMP protocol version for the input: MIDI1 = 0, MIDI2_V1 = 0
    // Currently unused.
    int32_t protocolVersion{0};
    // Returns the MIDI mapping policy flags that indicates the message types that the plugin may directly
    // consume and thus cannot be mapped to other purposes (parameter changes and preset changes).
    // The actual callee is most likely audio-plugin-host, not the plugin.
    midi_extension_get_mapping_policy_func_t get_mapping_policy;
} aap_midi2_extension_t;

#ifdef __cplusplus
} // extern "C"
#endif

#endif // ANDROIDAUDIOPLUGIN_MIDI_EXTENSION_H_INCLUDED
