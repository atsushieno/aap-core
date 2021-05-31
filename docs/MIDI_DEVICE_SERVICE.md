# Using AAP as MIDI Device Service

AAP Instrument plugin can be used as a MIDI device service if it is set up to behave so. It can be externally done outside the plugin application itself. An example is demonstrated as `aap-midi-device-service` application module.

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

