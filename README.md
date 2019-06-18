# AAP: Android Audio Plugin Framework

## What is AAP?

Android lacks commonly used Audio Plugin Framework. On Windows and other desktops, VSTs are popular. On Mac and iOS (including iPadOS) there is AudioUnit. On Linux LADSPA (either v1 or v2) is kind of used.

There is no such thing in Android. Android Audio Plugin (AAP) Framework is to fill this gap.

What AAP aims is to become like an inclusive standard for Android Audio plugin world. The license is permissive (MIT). It is designed to be pluggable to other specific audio plugin specifications like VST3, LV2, CLAP, and so on (not necessarily meant that *we* write code for them).

On the other hand it is designed so that cross-audio-plugin frameworks can support it. It would be possible to write backend and generator support for [JUCE](http://juce.com/) or [iPlug2](https://iplug2.github.io/).

Extensibility is provided like what LV2 does. VST3-specifics, or AAX-specifics, can be represented as long as it can be represented through raw pointer of any type (`void*`) i.e. cast to any context you'd like to have. Those extensions can be used only with supported hosts.

Technically it is not very different from LV2, but you don't have to spend time on learning RDF and Turtle to find how to create plugin description. Audio developers should spend their time on implementing or porting high quality audio processors. One single metadata query can achieve metadata generation.


## How AAPs work

AAP (Plugin) developers can ship their apps via Google Play (or any other app market). From app packagers perspective and users perspective, it can be distributed like a MIDI device service (but without Java dependency in audio processing).

AAP developers implement AndroidAudioPluginService which provides metadata on the audio plugins that it contains. It provides developer details, port details, and feature requirement details. (The plugins and their ports can NOT be dynamically changed, at least as of the first specification stage.)

It is very similar to what [AudioRoute](https://audioroute.ntrack.com/developer-guide.php) hosted apps do. (We are rather native oriented for performance reason, somewhat more seriously.)

Here is a brief workflow items for a plugin from the beginning, through processing audio and control (MIDI) inputs, to the end:

- initialize (pass sampleRate)
- prepare (pass initial pointers, which can be used to fix LV2 buffers)
- activate (DAW enabled it, playback is active or preview is active)
- process audio block (and/or control blocks)
- deactivate (DAW disabled it, playback is inactive and preview is inactive)
- terminate

This is quite like what LV2 does.

Unlike in-host-process plugin processing, it will become important to switch contexts. Considering the performance loss and limited resources on mobile devices, it is best if we can avoid that. However it is inevitable. It will be handled via [NdkBinder](https://developer.android.com/ndk/reference/group/ndk-binder).

An important note is that NdkBinder API is available only after Android 10 (Android Q). On earlier Android targets the binder must be implemented in Java API. At this state we are not sure if we support compatibility with old targets.

LADSPA and LV2 has somewhat unique characteristics - their port connection is established through raw I/O pointers. Since we support LV2 as one of the backends, the host at least give hint on "initial" pointers to each port, which can changed later at run time but basically only through setting changes (e.g. `connectPort()` for LV2), not at procecss time (e.g. `run()` for LV2). AAP expects plugin developers to deal with dynamically changed pointers at run time. We plugin bridge implementors have no control over host implementations or plugin implementations. So we have to deal with them within the bridged APIs (e.g. call `connectPort()` every time AAP `process()` is invoked with different pointers).

In any case, to pass direct pointers, Android [SharedMemory](https://developer.android.com/ndk/reference/group/memory) (ashmem) should play an important role here. There wouldn't be binary array transmits over binder IPC so far (but if it works better then things might change).


## AAP package bundle

AAP is packaged as a normal apk. The plugin is implemented in native code, built as a set of shared libraries.

The most important and difficult mission for an audio plugin framework is to get more plugins (hopefully more "quality" plugins, but that is next). Therefore AAP is designed to make existing things reusable. There are some packaging sub-formats e.g. AAP-VST3 or AAP-LV2, to ease plugin developers to reuse existing packaging system.

There is some complexity on how those files are packaged. At the "AAP package helpers" section we describe how things are packaged for each migration pattern.

### Queryable manifest

AAP plugins are not managed at system wide. Instead, AAP hosts can query AAPs using PackageManager which can look for specific services by intent filter `org.androidaudiopluginframework.AudioPluginService` and AAP "metadata". Here we follow what Android MIDI API does - AAP developers implement `org.androidaudiopluginframework.AudioPluginService` class and specify it as a `<service>` in `AndroidManifest.xml`. Here is an example

```
<manifest xmlns:android="http://schemas.android.com/apk/res/android"
          package="org.androidaudiopluginframework.samples.aapbarebonesample">
  ...
  <service android:name=".AudioPluginService"
           android:label="AAPBareBoneSamplePlugin">
    <intent-filter>
      <action 
	    android:name="org.androidaudiopluginframework.AudioPluginService" />
    </intent-filter>
    <meta-data 
	  android:name="org.androidaudiopluginframework.AudioPluginService"
	  android:resource="@xml/aap_metadata"
      />
  </service>
</manifest>
```

The `<service>` element comes up with a `<meta-data>` element. It is to specify an additional XML resource for the service. The `android:resource` attribute indicates that there is `res/xml/aap_metadata.xml` in the project. The file content should be like this:

```
<plugin manufacturer="AndroidAudioPluginProject"
		product="BareBoneSamplePlugin">
	<input-port name="MidiIn" content="midi" />
	<input-port name="ControlIn" content="control" />
	<input-port name="AudioIn" content="audio" />
	<output-port name="AudioOut" content="audio" />
</plugin>
```

The metadata format is super hacky for now and subject to change. The metadata content will be close to what LV2 metadata (`.ttl` file) provides.

AAP hosts can query AAP metadata resources from all the installed app packages, without instantiating those AAP services.


## AAP package helpers

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

Attempt to copy those `lv2` contents under `lib/{abi}` with simple build.gradle script failed. Asking plugin developers to add `copy(from/into)` operation hack (which might still not work) is awkward, so we would rather provide simpler solution - we put `lv2/` contents under `assets`, and put ABI-specific `*.so` files directly under `lib/{abi}`. Loading `*.so` from `assets` subdirectories is not possible either.

- `assets/foo.lv2/manifest.ttl`
- `assets/foo.lv2/foo.ttl`
- `lib/{abi}/foo.so`

The `copy-lv2-deps` target in the top `Makefile` does this task for the `poc-samples`.

Although, there is another big limitation on Android platform: it is not possible to get list of asset directories in Android, meaning that querying audio plugins based on the filesystem is not doable. All those plugins must be therefore explicitly listed at some manifest.

To workaround this issue, AAP-LV2 plugin takes a list of LV2 asset paths which are to build `LV2_PATH` settings. Note that it is done at plugin side. Host implementation has different story. (TODO)




### AAP-VST3

TBD - there should be similar restriction to that of AAP-LV2.



## Build Dependencies

android-audio-plugin-framework has some dependencies, which are either platform-level-specific, or external.

Platform wise:

- ashmem (Android 4.0.3)
- Realtime IPC binder (Android 8.0)
- AudioPluginService depends on NdkBinder (Android 10.0)

External software projects:

- lv2 category
  - lv2
    - libsndfile
      - libogg
      - libvorbis
      - flac
    - cairo
      - glib
      - libpng
      - zlib
      - pixman
      - fontconfig
      - freetype
  - lilv (private fork)
    - serd (private fork)
    - sord (private fork)
    - sratom
  - cerbero (as the builder, private fork)
- vst3 category
  - vst3sdk
    - TODO: fill the rest

The external dependencies are built using cerbero build system. Cerbero is a comprehensive build system that cares all standard Android ABIs and builds some complicated projects like glib (which has many dependencies) and cairo.

### LV2 forks

There are couple of lv2 related source repositories, namely serd and lilv. Their common assumption is that they have access to local files, which is not true about Android. They are forked from the original sources and partly rewritten to get working on Android.

And note that access to assets is not as simple as that to filesystem. It is impossible to enumerate assets at runtime. They have to be explicitly named and given.


## AAP hosting basics

AAP proof-of-concept host is in `poc-samples/AAPHostSample`.

AAP host will have to support multiple helper bridges e.g. AAP-LV2 and AAP-VST3. LV2 host bridge can be implemented using lilv.

Currently AAPHostSample contains *direct* LV2 hosting sample. It will be transformed to AAP hosting application.


### AAP hosting API

It is simpler than LV2. Similar to LV2, ports are connected only by index and no port instance structure for runtime (AAPPort is part of descriptor).

- Types - C API
  - `aap::PluginHostSettings`
  - `aap::PluginHost`
  - `aap::PluginHostBackend`
    - `aap::PluginHostBackendLV2`
    - `aap::PluginHostBackendVST3`
  - `aap::PluginInformation`
  - `aap::PortInformation`
  - `aap::PluginInstance`
  - `enum aap::BufferType { AAP_BUFFER_TYPE_AUDIO, AAP_BUFFER_TYPE_CONTROL }`
  - `enum aap::PortDirection { AAP_PORT_DIRECTION_INPUT, AAP_PORT_DIRECTION_OUTPUT }`

- Functions
  - host
    - `AAPHost* aap_host_create(AAPHostSettings*)`
    - `void aap_host_destroy(AAPHost*)`
    - `AAPPluginDesctiptorIterator* aap_host_get_plugins(AAPHost*)`
    - `AAPInstance* aap_host_instantiate(AAPHost*, AAPDesctiptor*)`
  - plugin desctiptor
    - `AAPPortIterator* aap_descriptor_get_ports(AAPDesctiptor*)`
  - iterators
    - `bool aap_desctiptor_iterator_next(AAPDesctiptorIterator*)`
    - `AAPDesctiptor* aap_desctiptor_iterator_current(AAPDesctiptorIterator*)`
    - `bool aap_port_iterator_next(AAPPortIterator*)`
    - `AAPPort* aap_port_iterator_current(AAPPortIterator*)`
  - plugin port desctiptor
    - `const char* aap_port_get_name(AAPPort*)`
    - `AAPBufferType aap_port_get_buffer_type(AAPPort*)`
    - `AAPPortDirection aap_port_get_direction(AAPPort*)`
  - plugin instance
    - `void aap_instance_connect(AAPInstance*)`
    - `void aap_instance_activate(AAPInstance*)`
    - `void aap_instance_run(AAPInstance*)`
    - `void aap_instance_deactivate(AAPInstance*)`


## JUCE integration

juce_android_audio_plugin_format.cpp implements juce:AudioPluginFormat and related stuff.


## android-audio-plugin-framework source tree structure

- README.md - this file.
- include - AAP C++ header files.
- src - AAP hosting reference implementation.
- juce - JUCE support project and sources.
- poc-samples - proof-of-concept Android app samples
  - AAPHostSample - host sample
  - AAPBareBoneSample - AAP (plugin) barebone sample
  - AAPLV2Sample - AAP-LV2 sample
  - AAPVST3Sample - AAL-VST3 sample
- external/cerbero - LV2 Android builder (reusing GStreamer's builder project, private fork of mine for LV2 recipes)


- native
  - libaap
    - include - AAP C/C++ header files
    - src - AAP hosting reference implementation (plugins don't have to reference anything. Packaging is another story though.)
  - juce - JUCE support project and sources.
- java
  - AndroidAudioPluginFramework
- poc-samples

