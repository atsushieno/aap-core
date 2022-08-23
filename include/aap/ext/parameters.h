
#ifndef ANDROIDAUDIOPLUGIN_PARAMETERS_EXTENSION_H_INCLUDED
#define ANDROIDAUDIOPLUGIN_PARAMETERS_EXTENSION_H_INCLUDED

#include "../android-audio-plugin.h"

#ifdef __cplusplus
extern "C" {
#endif

// This parameters extension assumes that parameter updates by host are sent to the plugin using
// MIDI2 inputs, and parameter change notification back to the host is sent from the plugin
// using MIDI2 outputs. The primary source for the inputs is a MIDI2 input port, and the destination
// (note that we do not say "primary" here) should be a MIDI2 output port.
//
// ## MIDI2-parameters mapping schemes
//
// The way how parameter changes and notifications are transmitted depends on get_mapping_policy().
// extension function.
// - When it returns `AAP_PARAMETERS_MAPPING_POLICY_ACC`, then the host can assume that parameter
//   changes requests from the host and the change notification to the host can be represented as
//   MIDI2 Assignable Controller (ACC) UMP.
// - When it returns `AAP_PARAMETERS_MAPPING_POLICY_ACC2`, it works quite like
//   AAP_PARAMETERS_MAPPING_POLICY_ACC, except that for AAP parameter changes and notifications
//   the most significant BIT of the ACC MSB is always 1 (i.e. always negative as int16_t),
//   so that the range of the valid parameter index is limited (but almost harmless in practive -
//   who has >32768 parameters?).
//   For this mode, plugins are responsible to remove tha flag when decoding the ACC UMP message.
//   It is useful IF the plugin should receive any MIDI message sent by host.
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

typedef struct aap_parameter_info_t {
    // Parameter ID.
    // For plugin users sake, parameter ID must be "stable", preserved for backward compatibility
    // (otherwise songs your users authored get broken and you plugin developers will be blamed).
    // Since there may be "dropped" indices, this does not mean the index in any parameter list.
    //
    // Type is int32_t so that any API that deals with parameters can return -1 for "not found" etc.
    int32_t stable_id;
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
