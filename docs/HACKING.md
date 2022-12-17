# Haking Tips

## Building this repo

Basically `./gradlew build` is the all-in-one command that handles everything. Or you can open the top directory on Android Studio. As of v0.7.4 it builds with Android Studio Dolphin, and we will basically keep compatibility with the stable version.

### Source tree structure

- `external` - native source dependencies.
- `include` - C include files. The top directory is for plugin API.
  - `core` - public API in `libandroidaudioplugin.so`.
    - `host` - AAP C++ header files for native host developers.
      - `android` - Android specific public API (regarding NdkBinder etc.)
- `androidaudioplugin` plugin framework and service implementation
- `androidaudioplugin-midi-device-service` MidiDeviceService implementation that receives MIDI inputs and sends to the AAP instrument.
- `androidaudioplugin-samples-host-engine` implements a plugin preview sample application that is also used as the default plugin preview template.
- `androidaudioplugin-ui-compose` implements the UI part of plugin preview template, based on the module above.
- `samples`
  - `aaphostsample` - sample host app - follows the same AGP-modules structure
  - `aapbarebonepluginsample` - sample plugin project, which can be used as a template. Audio processing part is no-op.
  - `aapinstrumentsample` - sample plugin project, which works as a reference implementation for instrument. It makes use of [true-grue/ayumi](https://github.com/true-grue/ayumi).
  - ` aap-midi-device-service` - sample MidiDeviceService project that lists all instruments.
 

## When Introducing Breaking Changes in the Core API.

It should very rately happen, but sometimes we would have to introduce breaking changes in the plugin API (`android-audio-plugin.h`), especially before 1.0 release. As the time of writing this, it happened in April 2022, and April 2020, and earlier (it is logged here to tell that it had been kept stable such long time).

Introducing breaking changes at the core API, unlike changes on the extension APIs, means that any of the existing plugins will be unusable and crashes at run-time if there is no safe guard to filter out such incompatibles oens.

(Incompatible extensions would just mean that the host will be unable to retrieve such an extension from the plugin, or the plugin will be unable to retrieve such an extension from the host. They might result in lack of mandatory extension or  some features disabled.)

When introducing breaking changes in the core API, we should alter the Android Intent action name. In the current API, it is `org.androidaudioplugin.AudioPluginService.V2`. Before August 2022, it was `org.androidaudioplugin.AudioPluginService.V1` and before April 2022, it was `org.androidaudioplugin.AudioPluginService`.
