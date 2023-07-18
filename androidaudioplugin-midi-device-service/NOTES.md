# androidaudioplugin-midi-device-service implementation notes

`AudioPluginMidiDeviceService` is a MidiDeviceService implementation that turns AAP instrument into a virtual MIDI output device (`MidiReceiver` in `android.media.midi` API).

As of Android 13, there is no API that makes it possible for a `MidiDeviceService` to receive MIDI inputs in native manner (Android Native MIDI API only expects `MidiDevice` instance as the entrypoint), every MIDI input is sent to `AudioPluginMidiDeviceService` via Android SDK API (Kotlin/JVM).

Here is the basic workflow:

Instantiation, mostly at Kotlin side:

- `AudioPluginMidiDeviceService` manages `AudioPluginMidiReceiver` instances for each port, in accordance with Android MIDI API.
- `AudioPluginMidiReceiver` internally holds an `AudioPluginMidiDeviceInstance` which internally holds an `AudioPluginClientBase` (which is kind of problematic; it should be probably managed at `AudioPluginMidiDeviceService` per service, but anyways) and a native instance (instantiated internally and held as instanceID).
- whenever `AudioPluginMidiReceiver` receives MIDI inputs, it is dispatched to native code via its `AudioPluginMidiDeviceInstance`.

MIDI message dispatching, at native side:

- `AudioPluginMidiDeviceInstance` simply passes the MIDI inputs it received to native `AAPMidiProcessor` using JNI.
- `AAPMidiProcessor::processMidiInput()` receives the MIDI 1.0 bytes input -
  - it internally translates it to UMP (`translateMidiBufferIfNeeded()`), then
  - resolves MIDI mapping to replace some messages (such as CCs) to parameter change sysex (`runThroughMidi2UmpForMidiMapping()`).
- the resulting UMP packets are stored in a temporary queue (buffer) that will be then copied to the actual MIDI port buffer when the plugin's `process()` is being kicked from within the Oboe audio callback.
- The Oboe callback is actually `oboe::StabilizedCallback` which forwards to platform-agnostic `AAPMidiProcessor::processAudioIO`. We also have "stub" implementation which sometimes helps debugging without depending on realtime audio.
- The audio callback (oboe `onAudioReady()` -> `AAPMidiProcessor::processAudioIO`) invokes the plugin instance's `process()` function and get audio outputs in non-interleaved form.
- Oboe audio callback needs to receive audio data as interleaved format, so interleave the `process()` output.
- Currently we put the audio processing results onto a `zix_ring_buffer` and let `processAudioIO()` consume it. (It should be unnecessarily though, but my attempt to remove it failed hard to trigger audio glitches. It is mostly harmless so I leave it so far.)


