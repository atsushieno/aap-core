
#ifndef ANDROIDAUDIOPLUGIN_PARAMETERS_EXTENSION_H_INCLUDED
#define ANDROIDAUDIOPLUGIN_PARAMETERS_EXTENSION_H_INCLUDED

#include "../android-audio-plugin.h"

#ifdef __cplusplus
extern "C" {
#endif

// This parameters extension assumes that parameter updates by host are sent to the plugin using
// MIDI2 inputs, and parameter change notification back to the host is sent from the plugin
// using MIDI2 outputs. The source input is a MIDI2 input port, and the destination
// should be a MIDI2 output port. There is the only one input and the only one output.
//
// ## MIDI2-parameters mapping schemes
//
// The way how parameter changes and notifications are transmitted depends on get_mapping_policy().
// extension function.
// - When it returns `AAP_PARAMETERS_MAPPING_POLICY_ACC`, then the host can assume that parameter
//   changes requests from the host and the change notification to the host can be represented as
//   MIDI2 Assignable Controller (ACC) UMP.
// - When it returns `AAP_PARAMETERS_MAPPING_POLICY_SYSEX8`, then it is one single Sysex8 packet
//   of 16 bits which looks like `5g sz pc 7E  7F 00 00 ch  k.n.idx.  value...`, where -
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
// - None does not have any premise. Host in general has no idea how to send the parameter changes.
//   Some specific hosts may be able to control such a plugin (for example, via some
//   Universal System Exclusive messages, or based on some Profile Configuration).
//   Or the plugin might want to expose all those parameters but not control and/or notify
//   changes publicly (but all at once, not per parameter?).
//
// They can also be Relative Assignable Controller, or Assignable Per-Note Controller, but it is
// up to the plugin if it accepts them or not.
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

#define AAP_PARAMETERS_EXTENSION_URI "urn://androidaudioplugin.org/extensions/parameters/v1"
#define AAP_PARAMETERS_XMLNS_URI "urn://androidaudioplugin.org/extensions/parameters"

#define AAP_MAX_PARAMETER_NAME_CHARS 256
#define AAP_MAX_PARAMETER_PATH_CHARS 256

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
