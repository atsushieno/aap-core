

# Haking Tips


## Building this repo

Basically `./gradlew build` is the all-in-one command that handles everything. Or you can open the top directory on Android Studio. Note that it is tailored for the latest Android Studio Canary (Dolphin-alpha at the time of writing this). You would still be able to open it on some older versions of Android Studio.

### Source tree structure

- `external` - native source dependencies. (It used to contain various LV2 related dependencies, but now they have gone to `aap-lv2` repo.)
- `include` - C include files. The top directory is for plugin API.
  - `core` - public API in `libandroidaudioplugin.so`.
    - `host` - AAP C++ header files for native host developers.
      - `android` - Android specific public API (regarding NdkBinder etc.)
-  Android modules
  - `androidaudioplugin` plugin framework and service implementation
  - `androidaudioplugin-samples-host-engine' implements a plugin preview sample application that is also used as the default plugin preview template.
  - `androidaudioplugin-ui-compose` implements the UI part of plugin preview template, based on the module above.
  - `androidaudioplugin-midi-device-service` MidiDeviceService implementation that receives MIDI inputs and sends to the AAP instrument.
  - `samples`
    - `aaphostsample` - sample host app - follows the same AGP-modules structure
    - `aapbarebonepluginsample` - sample plugin project, which can be used as a template. Audio processing part is no-op.
    - `aapinstrumentsample` - sample plugin project, which works as a reference implementation for instrument. It makes use of [true-grue/ayumi](https://github.com/true-grue/ayumi).
    - ` aap-midi-device-service` - sample MidiDeviceService project that lists all instruments.

 
### Running host sample app and plugin sample app

AAP is kind of client-server model, and to fully test AAP framework there should be two apps (for a client and a server). There are `aaphostsample` and `aapbarebonesample` within this repository, but `aap_lv2_mda` in [aap-lv2](https://github.com/atsushieno/aap-lv2) contains more complete plugin examples. Both host and plugins have to be installed to try out.

On Android Studio, you can switch apps within this repository, and decide which to launch.

There is also `AudioPluginHost` example in [aap-juce](https://github.com/atsushieno/aap-juce) which has more comprehensive plugin hosting features. It likely expose more buggy behaviors in the current souce code in either of the repositories.


## Fundamentals

The diagram below illustrates how remote plugins are instantiated. (NOTE: this diagram is partly outdated, and it gives wrong impression that we depend on JUCE. We don't; it is an example use case that shows how we imported JUCE AudioPluginHost in AAP world.)

![Instantiating remote plugins](images/aap-components.drawio.svg)


## Debugging Audio Plugins (Services)

It usually does not matter, but sometimes it does - when you would like to debug your AudioPluginServices, an important thing to note is that services (plugins) are not debuggable until you invoke `android.os.Debug.waitForDebugger()`, and you cannot invoke this method when you are NOT debugging (it will wait forever!) as long as the host and client are different apps. This also applies to native debugging via lldb (to my understanding).

`aappluginsample` therefore invokes this method only at `MainAcivity.onCreate()` i,e. it is kind of obvious that it is being debugged.

## Build with AddressSanitizer

When debugging AAP framework itself (and probably plugins too), AddressSanitizer (asan) is very helpful to investigate the native code issues. [The official Android NDK documentation](https://developer.android.com/ndk/guides/asan) should work as an up-to-date normative reference.

To enable asan in this project, there are three things to do:

- In this module:
  - run `./setup-asan-for-debugging.sh` to copy asan runtime shared libraries from NDK. You might have to adjust some variables in the script.
  - In the top-level `build.gradle.kts`, change `enable_asan` value to `true`. It will delegate the option to cmake as well as enable the ASAN settings in the library modules as well as sample apps.
- In your app module (or ours outside this repo e.g. `aap-juce-plugin-host/app`):
  - Similarly to `./setup-asan-for-debugging.sh` in this repo, you will have to copy ASAN libraries and `wrap.sh` into the app module. For more details, see [NDK documentation](https://developer.android.com/ndk/guides/asan).
    - **WARNING**: as of May 2022 there is a blocking NDK bug that prevents you from debugging ASAN-enabled code. See [this issue](https://github.com/android/ndk/issues/933) and find the latest script fix. We now have a temporary fix and actually use the script from there, but as it often happens, such a stale file could lead to future issues when the issue is fixed in the NDK upgrades. It should be removed whenever the issue goes away.
  - Add `android { packagingOptions { jniLibs { useLegacyPackaging = true } } }` (you would most likely have to copy "part of" this script in your existing build script e.g. only within `androoid { ... }` part)
  - Add `android:extractNativeLibs='true'` on `<application>` element in `AndroidManifest.xml`

Note that the ASAN options are specified only for `libandroidaudioplugin.so` and `libaapbareboneplugin.so`. To enable ASAN in other projects and their dependencies, pass appropriate build arguments to them as well.

## When Introducing Breaking Changes in the Core API.

It should very rately happen, but sometimes we would have to introduce breaking changes in the plugin API (`android-audio-plugin.h`), especially before 1.0 release. As the time of writing this, it happened in April 2022, and April 2020, and earlier (it is logged here to tell that it had been kept stable such long time).

Introducing breaking changes at the core API, unlike changes on the extension APIs, means that any of the existing plugins will be unusable and crashes at run-time if there is no safe guard to filter out such incompatibles oens.

(Incompatible extensions would just mean that the host will be unable to retrieve such an extension from the plugin, or the plugin will be unable to retrieve such an extension from the host. They might result in lack of mandatory extension or  some features disabled.)

When introducing breaking changes in the core API, we should alter the Android Intent action name. In the current API, it is `org.androidaudioplugin.AudioPluginService.V2`. Before August 2022, it was `org.androidaudioplugin.AudioPluginService.V1` and before April 2022, it was `org.androidaudioplugin.AudioPluginService`.
