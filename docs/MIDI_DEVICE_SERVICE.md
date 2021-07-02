!!! DRAFT DRAFT DRAFT !!!

# Using AAP as MIDI Device Service

AAP Instrument plugin can be used as a MIDI device service if it is set up to behave so. It is implemented by a class called `AudioPluginMidiDeviceService`.

It is usually packaged within the same instrument service app, but it can also be externally done outside the plugin application itself (the MidiDeviceService instantiates the plugin in the same manner regardless of whether it is in-app plugin or out of app plugin). An example is demonstrated as `aap-midi-device-service` application module.

AAP Effect plugins do not work with this feature (there is no point of making them so).

## MidiDeviceService setup how-to

(1) Add `<service>` element in `AndroidManifest.xml`:

```
<service android:name="org.androidaudioplugin.midideviceservice.AudioPluginMidiDeviceService"
    android:permission="android.permission.BIND_MIDI_DEVICE_SERVICE"
    android:exported="true">
    <intent-filter>
        <action android:name="android.media.midi.MidiDeviceService" />
    </intent-filter>
    <meta-data android:name="android.media.midi.MidiDeviceService"
        android:resource="@xml/midi_device_info" />
</service>
```

Here we specify `midi_device_info.xml` as the MidiDeviceService descriptor.

(2) `res/xml/midi_device_service.xml` (can be any XML file)

It is requierd by Android framework to put in `res/xml` directory, like:

```
<devices>
    <device name="aap-ayumi" manufacturer="androidaudioplugin.org" product="aap-ayumi">
        <input-port name="input" />
    </device>
</devices>
```

The file name must match the one specified in `AndroidManifest.xml`.

It is just compliant to Android MIDI API. The metadata should be modified to match the product you want to turn into a MIDI device service.

## Design limitations

We cannot dynamically add or remove `<device>`s in the meta-data XML. Therefore, whenever you want to instantiate an arbitrary AAP instrument, there is no way to designate particular AAP in the `<device>` element.

For that reason, we have leave `<device>` elements AAP-agnostic. On the other hand, we also have to provide ways to associate an AAP to a `<device>` element in case there are more than one instrument plugins in a service (consider aap-mda-lv2).

## MIDI 1.0/2.0 protocols

AudioPluginMidiDeviceService internally uses MIDI 2.0 UMP to send MIDI input messages to the instrument plugin, and the instrument plugin is supposed to downconvert to MIDI 1.0 stream if it cannot handle UMPs. aap-lv2 does this automatically. aap-juce plugins do not.

AudioPluginMidiDeviceService is instantiated without protocol information, and it receives MIDI inputs without being given the protocol.

Ideally it would consume some MIDI CI Protocol messages to switch those message types, but at this state there is no correlated feedback channel to send MIDI CI responses to the client.

### MIDI 1.0 or 2.0 between app and MidiDeviceService

`aap-midi-device-service` app has a checkbox that says "Use MIDI 2.0 Protocol". Its scope is limited to determine whether it sends UMP messages or traditional MIDI 1.0 bytes, not to control whether the MidiDeviceService should use MIDI 2.0 Protocols or not for its internal interaction with AAP instrument. For the reason explained above, a MIDI client has no control over which MIDI protocol `AudioPluginMidiDeviceService` uses.
