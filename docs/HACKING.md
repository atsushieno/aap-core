

# Haking Tips


## Building this repo

Basically `make` is the all-in-one command that handles everything on Unix-y environment. It will perform some supplemental tasks before it runs `gradlew`. You will have to specify `ANDROID_SDK_ROOT` before running `make`.

Android NDK will be downloaded to `~/Android/Sdk/ndk/{rev}` unless `ANDROID_NDK` variable is externally specified to `make`.

gradle is used as **part of** the entire builds, for Android application samples and libraries.

Once supplemental things are set up, Android Studio can be used for development by opening `java` directory.

### Source tree structure

-  `aidl` - contains common internal interface definitions for native and Kotlin modules.
-  `dependencies` - native source dependencies. (It used to contain various LV2 related dependencies, but now they have gone to `aap-lv2` repo.)
-  `native`
  -  `plugin-api` (C include file; it is just for reference for plugin developers)
  -  `core`
    -  `include` - AAP C++ header files for native host developers. (Plugins don't have to reference anything. Packaging is another story though.)
    -  `src` - AAP hosting reference implementation
  -  `android` (Android-specific parts; NdkBinder etc.)
  -  `desktop` (general Unix-y desktop specific parts; POSIX-IPC maybe)
-  `java`
  -  `androidaudioplugin` plugin framework and service implementation
  -  `androidaudioplugin-ui-androidx` plugin UI implementation helper (not mandatory), based on traditional android.widget.
  -  `samples`
    -  `aaphostsample` - sample host app - follows the same AGP-modules structure
    -  `aapbarebonepluginsample` - sample plugin project, which can be used as a template. Audio processing part is no-op.

 
### Running host sample app and plugin sample app

AAP is kind of client-server model, and to fully test AAP framework there should be two apps (for a client and a server). There are `aaphostsample` and `aapbarebonesample` within this repository, but `aap_lv2_mda` in [aap-lv2](https://github.com/atsushieno/aap-lv2) contains more complete plugin examples. Both host and plugins have to be installed to try out.

On Android Studio, you can switch apps within this repository (the project in `java` directory), and decide which to launch.

There is also `AudioPluginHost` example in [aap-juce](https://github.com/atsushieno/aap-juce) which has more comprehensive plugin hosting features. It likely expose more buggy behaviors in the current souce code in either of the repositories.


## Fundamentals

The diagram below illustrates how remote plugins are instantiated.

![Instantiating remote plugins](images/aap-components.drawio.svg)


## Debugging Audio Plugins (Services)

It usually does not matter, but sometimes it does - when you would like to debug your AudioPluginServices, an important thing to note is that services (plugins) are not debuggable until you invoke `android.os.Debug.waitForDebugger()`, and you cannot invoke this method when you are NOT debugging (it will wait forever!) as long as the host and client are different apps. This also applies to native debugging via lldb (to my understanding).

`aappluginsample` therefore invokes this method only at `MainAcivity.onCreate()` i,e. it is kind of obvious that it is being debugged.

## Build with AddressSanitizer

When debugging AAP framework itself (and probably plugins too), AddressSanitizer (asan) is very helpful to investigate the native code issues. To enable asan, we have additional lines in `build.gradle` wherever NDK-based native builds are involved:

```
android {
    defaultConfig {
        externalNativeBuild {
            cmake {
                arguments "-DANDROID_ARM_MODE=arm", "-DANDROID_STL=c++_shared"
                cppFlags "-fsanitize=address -fno-omit-frame-pointer"
            }
        }
    }
```

Depending on the development stage they are either enabled or not. Stable releases should have these lines commented out.

Note that the ASAN options are specified only for libandroidaudioplugin.so and libaaphostsample.so. To enable ASAN in dependencies, pass appropriate build arguments to them as well.


