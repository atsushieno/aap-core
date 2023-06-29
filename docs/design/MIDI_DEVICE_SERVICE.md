# Using AAP as MIDI Device Service

AAP Instrument plugin can be used as a MIDI device service if it is set up to behave so. It is implemented by a class called `AudioPluginMidiDeviceService`.

It internally instantiates `AudioPluginMidiReceiver` as its MIDI output port `MidiReceiver` (Note: In Android MIDI Device Service API, "output" is implemented as `MidiReceiver`. Everything is flipped, just like `javax.sound.midi` API).

It is usually packaged within the same instrument service app, but it can also be externally done outside the plugin application itself (the MidiDeviceService instantiates the plugin in the same manner regardless of whether it is in-app plugin or out of app plugin).

AAP Effect plugins do not work with this feature (there is no point of making them so). In theory it is possible to chain effect plugins after another instrument plugin, but that is out of scope so far (you can build your own MidiDeviceService as such).

## MIDI mapping settings

By default, AudioPluginMidiDeviceService implementation applies some MIDI mappings from MIDI 2.0 messages to particular plugin operations.

In short: you can set parameters via CCs, ACCs (NRPN) and/or Per-Note ACCs, and you can set presets via Program Changes, by default. Or, more precisely:

- Control Changes are mapped to parameter changes. CC index (0-127) indicates parameter index, and CC data (32-bit) indicates the parameter value.
- Assignable Controllers (ACCs, NRPNs) and Per-Note Assignable Controllers (PNACCs) are mapped to parameter changes. MSB and LSB combined together represent the parameter index (0-0x3FFF) and ACC/PNACC data value (32-bit) indicates the parameter value.
- Program Changes are mapped to set preset index. Program (0-127) along with bank select indicates preset index. Bank MSB and LSB combined together represent higher digits than 0x80 for the index i.e. index = (Bank MSB * 0x80 + Bank LSB) * 0x80 + program.

Note that they are in MIDI 2.0 semantics. As of current version, Android MidiDeviceService only accepts MIDI 1.0 inputs so they are first translated to MIDI 2.0 UMPs and then mapped along with the rules above.

Also, MIDI 2.0 32-bit data is integer, but should be interpreted as float (raw bits are assigned, no type conversions in their value semantics happen).

It is actually up to plugin whether they process those MIDI inputs by its own. It is configurable by AAP parameters extension `get_mapping_policy` function and the plugin settings in AudioPluginService (stored as its shared preference).

## MidiDeviceService setup how-to

(1) In `build.gradle(.kts)`, add `androidaudioplugin-midi-device-service` as a dependency:

```
dependencies {
    implementation ("org.androidaudioplugin:androidaudioplugin-midi-device-service:+") // replace + with the actual version
}
```

(2) Just as [a standard MidiDeviceService](https://developer.android.com/reference/android/media/midi/package-summary#manifest_files), add `<service>` element in `AndroidManifest.xml`:

```
<service android:name="org.androidaudioplugin.midideviceservice.StandaloneAudioPluginMidiDeviceService"
    android:permission="android.permission.BIND_MIDI_DEVICE_SERVICE"
    android:exported="true">
    <intent-filter>
        <action android:name="android.media.midi.MidiDeviceService" />
    </intent-filter>
    <meta-data android:name="android.media.midi.MidiDeviceService"
        android:resource="@xml/midi_device_info" />
</service>
```

Here in the `<meta-data>` element, we specify `midi_device_info.xml` as the MidiDeviceService descriptor.

In this example it uses `StandaloneAudioPluginMidiDeviceService` class which limits the target plugin to the ones within the same application (i.e. "local" plugins), but you can create your own derived class from `AudioPluginMidiDeviceService` and determine your target plugins.

(3) Similarly as Android Framework requires, add `res/xml/midi_device.xml` (can be any XML filename, it has to match the resource name indicated above):

It is requierd by Android framework to put in `res/xml` directory, like:

```
<devices>
    <device name="aap-ayumi" manufacturer="androidaudioplugin.org" product="aap-ayumi">
        <input-port name="input" />
    </device>
</devices>
```

By default, `AudioPluginMidiDeviceService` class assumes that your plugin's display name starts with the device name specified here, and ends with the port name specified here. Another assumption is that if there is only one plugin in scope, then the plugin is used.

The file name must match the one specified in `AndroidManifest.xml`.

It is just compliant to Android MIDI API. The metadata should be modified to match the product you want to turn into a MIDI device service.

## Design limitations

First of all, there seems no way to specify multiple `<device>` elements within the top-level `<devices>` element in the `midi_device_info.xml` (or whatever you name). As far as I tried, only the first `<device>` element is recognized (see [the relevant issue](https://github.com/atsushieno/aap-core/issues/91)).

Also We cannot dynamically add or remove `<port>`s in the meta-data XML. Therefore, whenever you want to instantiate an arbitrary AAP instrument, you have to designate particular AAPs in the `<port>` elements in prior.

For that reason, we leave `<port>` elements AAP-agnostic. On the other hand, we also have to provide ways to associate an AAP to a `<port>` element in case there are more than one instrument plugins in a service (see `aap-mda-lv2` module in `aap-lv2-mda` project).

## MIDI 1.0/2.0 protocols

On the public frontend, `AudioPluginMidiReceiver` assumes that the incoming MIDI messages conforms to MIDI 1.0 by default (which would be the most general assumption).
It can promote to MIDI 2.0 protocol if it received MIDI CI "Set New Protocol" Universal SysEx message. It is the only way that an arbitrary application can tell AudioPluginMidiReceiver to tell the receiver that it will be sending MIDI 2.0 UMPs.

Regardless of whether `AudioPluginMidiReceiver` receives the messages in MIDI1 or MIDI2 packets, it *internally* uses MIDI 2.0 UMP to send MIDI input messages to the instrument plugin i.e. it upconverts MIDI1 messages to MIDI2.
And the instrument plugin is supposed to downconvert to MIDI 1.0 stream if it cannot handle UMPs. aap-lv2 and aap-juce do this automatically.

(We once decided to not always use MIDI 2.0 UMPs. It was then up to the actual plugins that processe MIDI messages, except for dynamic protocol changes (therefore we inspect the raw bytes). But with AAP "V2" protocol, it is gone. We always have MIDI2 port.)

AudioPluginMidiDeviceService is instantiated without protocol information, and it receives MIDI inputs without being given the protocol.

Ideally it would consume some MIDI CI Protocol messages to switch those message types, but at this state there is no correlated feedback channel to send MIDI CI responses to the client.

### MIDI 1.0 or 2.0 between app and MidiDeviceService

The MidiDeviceService can switch protocol between MIDI 1.0 bytestream and MIDI 2.0 UMPs (protocol *within* UMP is fixed to MIDI2 so far). To switch the protocol, you have to send UMP Stream Configuration Request (F0 05 xx yy ...) in platform native order. See [issue #161](https://github.com/atsushieno/aap-core/issues/161) for details.

Like MIDI-CI Set New Protocol message, our implementation expects that UMP Stream Configuration Request message sent without being attached to any other UMPs, so that the packets do not get mis-interpreted in the former protocol.

As of writing this, there is no notification sent back to the "initiator" as (currently) MidiDeviceService is not designed to be bi-directional and thus there is no input port ("output port" in Android MIDI API wording).

Note that this does not affect AAP MIDI processing which is *always* done in UMPs. It is only about users' MIDI client app to this MidiDeviceService.

