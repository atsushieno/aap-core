# Haking Tips

## Building this repo

Basically `./gradlew build` is the all-in-one command that handles everything. Or you can open the top directory on Android Studio. As of v0.7.4 it builds with Android Studio Dolphin, and we will basically keep compatibility with the stable version.

## modules in this repo

### androidaudioplugin

This library provides AudioPluginService which is mandatory for every AAP (plugin), as well as some hosting functionality.

### androidaudioplugin-midi-device-service

AAP instrument plugins can be extended to work as a virtual MIDI device service (software MIDI device). It turns any instrument plugin into a virtual MidiDeviceService.

There used to be `aap-midi-device-service` sample application ([#149](https://github.com/atsushieno/aap-core/issues/149)).

### androidaudioplugin-ui-compose-app

`androidaudioplugin-ui-compose-app` module contains `PluginManagerActivity` which can be used as a launcher activity by any audio plugin application.

It lists the plugins within the package, and when selected it provides MIDI messaging via the keyboard, or apply effects to some audio source (to be extended, but fixed to a file so far).

The UI is based on Jetpack Compose.

It depends on `androidaudioplugin-manager` internal component to provide "plugin player" feature (we may have more features, or move everything into this ui-compose-app aar).

It is also used by samples/aaphostsample that lists **all** plugins on the system.

### androidaudioplugin-ui-web

Experimental Web UI bits that may or may not be used in the future. To be documented.

### aaphostsample and aapbarebonepluginsample

`aaphostsample` lists existing AAPs (services) that are installed on the same device, and demonstrates how they work, using some raw audio data example as well as some example MIDI input messages. As of writing this, the app does not respect any audio format and processes in fixed size, and sends some fixed MIDI messages (note on/off-s) to AAP plugins.

`aapbarebonepluginsample` is a sample AAP (plugin) that does "nothing". It can be rather used as a plugin template project.

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

## When Introducing Breaking Changes in the Core API.

It should very rately happen, but sometimes we would have to introduce breaking changes in the plugin API (`android-audio-plugin.h`), especially before 1.0 release. As the time of writing this, it happened in April 2022, and April 2020, and earlier (it is logged here to tell that it had been kept stable such long time).

Introducing breaking changes at the core API, unlike changes on the extension APIs, means that any of the existing plugins will be unusable and crashes at run-time if there is no safe guard to filter out such incompatibles oens.

(Incompatible extensions would just mean that the host will be unable to retrieve such an extension from the plugin, or the plugin will be unable to retrieve such an extension from the host. They might result in lack of mandatory extension or  some features disabled.)

When introducing breaking changes in the core API, we should alter the Android Intent action name. In the current API, it is `org.androidaudioplugin.AudioPluginService.V3`. Before February 2023, it was it is `org.androidaudioplugin.AudioPluginService.V2`, and before August 2022, it was `org.androidaudioplugin.AudioPluginService.V1` and before April 2022, it was `org.androidaudioplugin.AudioPluginService`.
