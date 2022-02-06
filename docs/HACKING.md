

# Haking Tips


## Building this repo

Basically `make` is the all-in-one command that handles everything on Unix-y environment. It will perform some supplemental tasks before it runs `gradlew`. You will have to specify `ANDROID_SDK_ROOT` before running `make`.

Android NDK will be downloaded to `~/Android/Sdk/ndk/{rev}` unless `ANDROID_NDK` variable is externally specified to `make`.

gradle is used as **part of** the entire builds, for Android application samples and libraries.

Once supplemental things are set up, Android Studio can be used for development by opening the top directory.

### Source tree structure

- `aidl` - contains common internal interface definitions for native and Kotlin modules.
- `dependencies` - native source dependencies. (It used to contain various LV2 related dependencies, but now they have gone to `aap-lv2` repo.)
- `native`
  - `plugin-api` (C include file; it is just for reference for plugin developers)
  - `core`
    - `include` - AAP C++ header files for native host developers. (Plugins don't have to reference anything. Packaging is another story though.)
    - `src` - AAP hosting reference implementation
  - `android` (Android-specific parts; NdkBinder etc.)
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

The diagram below illustrates how remote plugins are instantiated.

![Instantiating remote plugins](images/aap-components.drawio.svg)


## Debugging Audio Plugins (Services)

It usually does not matter, but sometimes it does - when you would like to debug your AudioPluginServices, an important thing to note is that services (plugins) are not debuggable until you invoke `android.os.Debug.waitForDebugger()`, and you cannot invoke this method when you are NOT debugging (it will wait forever!) as long as the host and client are different apps. This also applies to native debugging via lldb (to my understanding).

`aappluginsample` therefore invokes this method only at `MainAcivity.onCreate()` i,e. it is kind of obvious that it is being debugged.

## Build with AddressSanitizer

When debugging AAP framework itself (and probably plugins too), AddressSanitizer (asan) is very helpful to investigate the native code issues.

To enable asan in this project, there are three things to do:

- run `./setup-asan-for-debugging.sh` to copy asan runtime shared libraries from NDK. You might have to adjust some variables.
- uncomment some lines in `CMakeLists.txt` (search for "AddressSanitizer" to find the corresponding lines.)
- Add `android:extractNativeLibs='true'` on `<application>` element in `AndroidManifest.xml`

Note that this description is not equivalent to what [NDK documentation](https://developer.android.com/ndk/guides/asan) says; its `wrap.sh` is broken and you'd stuck at unresponsive debugging.

Note that the ASAN options are specified only for `libandroidaudioplugin.so` and `libaapbareboneplugin.so`. To enable ASAN in other projects and their dependencies, pass appropriate build arguments to them as well.
