# AAP: Android Audio Plugin Framework

## What is AAP?

Android lacks Audio Plugin Framework. On Windows and other desktops, VSTs are popular. On Mac and iOS (including iPadOS) there is AudioUnit. On Linux LADSPA (either v1 or v2) is kind of used.

There is no such thing in Android. Android Audio Plugin (AAP) Framework is to fill this gap.

What AAP aims is to become like an inclusive standard for Android Audio plugin world. The license is permissive (MIT). It is designed to be pluggable to other specific audio plugin specifications like VST3, LV2, CLAP, and so on (not necessarily meant that *we* write code for them).

On the other hand it is designed so that cross-audio-plugin frameworks can support it. It would be possible to write backend and generator support for [JUCE](http://juce.com/) or [iPlug2](https://iplug2.github.io/).

Extensibility is provided like what LV2 does. VST3-specifics, or AAX-specifics, can be represented as long as it can be represented through raw pointer of any type (`void*`) i.e. cast to any context you'd like to have.

Technically it is not very different from LV2, but you don't have to spend time on learning RDF and Turtle to find how to create plugin description. Audio developers should spend their time on implementing or porting high quality audio processors. One single metadata query can achieve metadata generation.


## How AAP Works

AAP (Plugin) developers can ship their apps via Google Play (or any other app market). From app packagers perspective and users perspective, it can be distributed like a MIDI device service (but without Java dependency in audio processing).

AAP developers implement AndroidAudioPluginService which provides metadata on the audio plugins that it contains. It provides developer details, port details, and feature requirement details. (The plugins and their ports can be dynamically changed, so the query should come up with certain timestamp that is used as a threshold to determine if a plugin needs updated information since last query.)

It is very similar to what [AudioRoute](https://audioroute.ntrack.com/developer-guide.php) hosted apps do. (We are rather native oriented to consider performance somewhat more seriously.)

Here is a brief workflow items for a plugin from the beginning, through processing audio and control (MIDI) inputs, to the end:

- initialize
- prepare (connect ports)
- activate (DAW enabled it, playback is active or preview is active)
- process audio block (and/or control blocks)
- deactivate (DAW disabled it, playback is inactive and preview is inactive)
- terminate

This is quite like what LV2 does.

Unlike in-host-process plugin processing, it will become important to switch contexts. Considering the performance loss and limited resources on mobile devices, it is best if we can avoid that. However it is inevitable. It will be handled via [NdkBinder](https://developer.android.com/ndk/reference/group/ndk-binder).

Similarly to LV2, port connection is established as setting raw I/O pointers. Android [SharedMemory](https://developer.android.com/ndk/reference/group/memory) (ashmem) should play an important role here. There wouldn't be binary array transmits over binder IPC so far, but if it works better then things might change.


## AAP package bundle

AAP is packaged as a normal apk. The plugin is implemented in native code, built as a set of shared libraries.

The most important and difficult mission for an audio plugin framework is to get more plugins (hopefully more "quality" plugins, but that is next). Therefore AAP is designed to make existing things reusable. There are some packaging sub-formats e.g. AAP-VST3 or AAP-LV2, to ease plugin developers to reuse existing packaging system.

There is some complexity on how those files are packaged. The following sections describe how things are packaged for each packaging pattern.

### AAP-LV2

LV2 packaging is not straightforward. Android native libraries are usually packaged like

- `lib/armeabi-v7a/libfoo.so`
- `lib/arm64-v8a/libfoo.so`
- `lib/x86/libfoo.so`
- `lib/x86_64/libfoo.so`

while normal `lv2` packages usually look like:

- `lib/foo.lv2/manifest.ttl`
- `lib/foo.lv2/foo.ttl`
- `lib/foo.lv2/foo.so`

At this state we are not sure if keeping `.lv2` directory like this is doable. It is not doable to support multiple ABIs with this directory layout anyways.


### AAP-VST3

TBD - there should be similar restriction to that of AAP-LV2.


## android-audio-plugin-framework source tree structure

- README.md - this file.
- include - AAP C++ header files.
- src - AAP hosting reference implementation.
- juce - JUCE support project and sources.
- poc-samples - proof-of-concept Android app samples
  - AAPHost - host sample
  - AAPBareBoneSample - AAP (plugin) barebone sample
  - AAPLV2Sample - AAP-LV2 sample
  - AAPVST3Sample - AAL-VST3 sample
