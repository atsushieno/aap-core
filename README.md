
# AAP: Android Audio Plugin Framework

![](https://github.com/atsushieno/android-audio-plugin-framework/workflows/build%20dist/badge.svg)

  

[![AAP demo 20200708](http://img.youtube.com/vi/gKCpHvYzupU/0.jpg)](http://www.youtube.com/watch?v=gKCpHvYzupU  "AAP demo 20200708")

disclaimer: the README is either up to date, partially obsoleted, or sometimes (but not very often) ahead of implementation (to describe the ideal state). Do not trust it too much.

## What is AAP?

Android lacks commonly used Audio Plugin Framework. On Windows and other desktops, VSTs are popular. On Mac and iOS (including iPadOS) there is AudioUnit. On Linux LV2 is used, to some extent.

There is no such thing in Android. Android Audio Plugin (AAP) Framework is to fill this gap.

What AAP aims is to become like an inclusive standard for audio plugin, adoped to Android applications ecosystem. The license is permissive (MIT). It is designed to be pluggable from other specific audio plugin specifications like VST3, LV2, CLAP, and so on (not necessarily meant that *we* write code for them).

On the other hand it is designed so that cross-audio-plugin SDKs can support it. We have [JUCE](http://juce.com/) integration support, and once [iPlug2](https://iplug2.github.io/) supports Linux and Android it would become similarly possible. [DPF](https://github.com/DISTRHO/DPF) too. Namely, AAP is first designed so that JUCE audio processor can be implemented and JUCE-based audio plugins can be easily imported to AAP world.

We have [aap-lv2](https://github.com/atsushieno/aap-lv2) and [aap-juce](https://github.com/atsushieno/aap-juce/) repositories that achieve these objectives, to some extent. We have some plugins from these world working (to some extent) - for example: [mda-lv2](https://drobilla.net/software/mda-lv2), [Fluidsynth](https://github.com/FluidSynth/fluidsynth) (as aap-fluidsynth), [sfizz](https://github.com/sfztools/sfizz/), [Guitarix](https://github.com/brummer10/guitarix) in aap-lv2,  [Dexed](https://asb2m10.github.io/dexed/), [OPNplug](https://github.com/jpcima/ADLplug), [OB-Xd](https://github.com/reales/OB-Xd), and [JUCE AudioPluginHost](https://github.com/juce-framework/JUCE/tree/master/extras/AudioPluginHost) in aap-juce. Note that there is no plugin UI integration support yet.

## AAP features, characteristics, unique points

**Android is supported, and it is the first citizen** : no other audio plugin frameworks achieve that (except for [AudioRoute SDK](https://github.com/AudioRoute/AudioRoute-SDK), as far as @atsushieno knows). To make it possible, we have some other characteristics explained below.

**out-process model, between host activities and plugin services** : AAP is designed to work for Android platform, which has strict separation on each application process space. Namely, we cannot load arbitrary shared libraries from random plugins. Thus AAP DAWs (plugin hosts) and AAPs (plugins) have to communicate through IPC mechanism. AAP uses Binder IPC through NdkBinder API which was introduced at Android 10. Also, the framework makes full use of Android shared memory (ashmem) throughout the audio/MIDI buffer processing.

![AAP process model](docs/images/aap-process-model.png)

**Extensibility** : plugin feature extensibility is provided through raw pointer data. But it must not contain function pointers, as Android platform does not support calling them from different apps.

**Basically declarative parameter meta data** : like LV2, we expect plugin metadata `res/xml/aap_metadata.xml`, described its ports.

**C/C++ and Kotlin supported**: C/C++ is supported for plugin and hosting, Kotlin for hosting.

**Permissive licensing** : It is released under the MIT license. Similar to LV2 (ISC), unlike VST3 or JUCE (GPLv3).

**API unstability** : unlike other audio plugin frameworks, we don't really offer API stability. What we recommend instead is to use APIs from audio plugin framework or SDKs, such as JUCE or LV2 API, and port them to AAP. AAP will be API stable "at some stage", but that is not planned.

**Linux desktop builds** : developing plugins on Android targets can be annoying, so we also provide the way to develop plugins on Linux desktop too. It may be extended to Mac and even Windows perhaps. However it is for development purpose; we don't aim to replace existing plugin frameworks like LV2 or VST3.


## How AAPs work: technical background

AAP distribution structure is simple; both hosts (DAWs) and plugins (instruments/effects) can be shipped as Android applications (via Google Play etc.) respectively:

- AAP Host (DAW) developers can use AAP hosting API to query and load the plugins, then deal with audio data processed by them.
- AAP (Plugin) developers works as a service, processes audio and MIDI messages.

From app packagers perspective and users perspective, it can be distributed like a MIDI device service. Like Android Native MIDI (introduced in Android 10.0), AAP processes all the audio stuff in native land (it still performs metadata queries and service queries in Dalvik/ART land).

AAP developers create audio plugin in native code using Android NDK, create plugin "metadata" as an Android XML resource (`aap_metadata.xml`), and optionally implement `org.androidaudioplugin.AudioPluginService` which handles audio plugin connections using Android SDK, then package them together. The metadata provides developer details, port details (as long as they are known), and feature requirement details.

TODO: The plugins and their ports can NOT be dynamically changed, at least as of the current specification stage. We should seriously reconsider this. It will be mandatory when we support so-called plugin wrappers.

AAP is similar to what [AudioRoute](https://audioroute.ntrack.com/developer-guide.php) hosted apps do. We are rather native oriented for reusing existing code. (I believe they also aim to implement plugins in native code, but not sure. The SDK is not frequently updated.)


## How to create AAP plugins

[Developers Guide](DEVELOPERS.md) contains some essential guide and some details on various topics.

We have a dedicated plugin development guide for (1) [building from scratch](PLUGIN_SCRATCH_GUIDE.md), (2) [importing from LV2 plugins](https://github.com/atsushieno/aap-lv2), and (3) [importing from JUCE plugins](https://github.com/atsushieno/aap-juce). We basically recommend (2) or (3) as our API is not quite stabilized yet. So if you take the (1) approach, then you are supposed to update source code whenever necessaey.

(We don't really assure the consistency on how we import LV2 and JUCE bits either, but their API would be mostly stable.)

 
## Sample apps

### aaphostsample and aapbarebonepluginsample

`aaphostsample` lists existing AAPs (services) that are installed on the same device, and demonstrates how they work, using some raw audio data example as well as some example MIDI input messages. As of writing this, the app does not respect any audio format and processes in fixed size, and sends some fixed MIDI messages (note on/off-s) to AAP plugins.

`aapbarebonepluginsample` is a sample AAP (plugin) that does "nothing". It can be rather used as a plugin template project.

The waveform rendering in `aaphostsample` is done thanks to [waveformSeekBar](https://github.com/massoudss/waveformSeekBar).

## Further documentation

[DEVELOPERS.md](docs/DEVELOPERS.md) contains more on plugin and host development.

[PLUGIN_SCRATCH_GUIDE.md](docs/PLUGIN_SCRATCH_GUIDE.md) contains how to get started with plugin development using our own (unstable!) API.

[HACKING.md](docs/HACKING.md) contains how to build and hack this framework itself.

[DESIGN_NOTES.md](docs/DESIGN_NOTES.md) contains some technical background and design decisions that we have made in this framework.


