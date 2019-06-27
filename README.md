# AAP: Android Audio Plugin Framework

## What is AAP?

Android lacks commonly used Audio Plugin Framework. On Windows and other desktops, VSTs are popular. On Mac and iOS (including iPadOS) there is AudioUnit. On Linux LADSPA (either v1 or v2) is used.

There is no such thing in Android. Android Audio Plugin (AAP) Framework is to fill this gap.

What AAP aims is to become like an inclusive standard for Audio plugin, adoped to Android applications ecosystem. The license is permissive (MIT). It is designed to be pluggable to other specific audio plugin specifications like VST3, LV2, CLAP, and so on (not necessarily meant that *we* write code for them).

On the other hand it is designed so that cross-audio-plugin frameworks can support it. It would be possible to write backend and generator support for [JUCE](http://juce.com/) or [iPlug2](https://iplug2.github.io/). Namely, AAP is first designed so that JUCE audio processor can be implemented and JUCE-based audio plugins can be easily imported.

Extensibility is provided like what LV2 does. VST3-specifics, or AAX-specifics, can be represented as long as it can be represented through raw pointer of any type (`void*`) i.e. cast to any context you'd like to have. Those extensions can be used only with supported hosts. A host can query each plugin whether it supports certain feature or not and disable those not-supported plugins.

Technically it is not very different from LV2, but you don't have to spend time on learning RDF and Turtle to start actual plugin development (you still have to write manifests in XML). Audio developers should spend their time on implementing or porting high quality audio processors.



## How AAPs work

AAP (Plugin) developers can ship their apps via Google Play (or any other app market). From app packagers perspective and users perspective, it can be distributed like a MIDI device service. Like Android Native MIDI (introduced in Android 10.0), AAP processes all the audio stuff in native land (it still performs metadata queries in Dalvik land).

AAP developers implement `org.androidaudiopluginframework.AudioPluginService` which handles audio plugin connections, and create plugin "metadata" in XML. The metadata provides developer details, port details, and feature requirement details. (The plugins and their ports can NOT be dynamically changed, at least as of the first specification stage.)

AAP is similar to what [AudioRoute](https://audioroute.ntrack.com/developer-guide.php) hosted apps do. (We are rather native oriented for performance and ease of reusing existing code, somewhat more seriously.)

### plugin API

From each audio plugin's point of view, it is locally instantiated by each service application. Here is a brief workflow items for a plugin from the beginning, through processing audio and MIDI inputs (or any kind of controls), to the end:

- get plugin factory
- instantiate plugin (pass plugin ID and sampleRate)
- prepare (pass initial pointers, which can be used to fix LV2 buffers)
- activate (DAW enabled it, playback is active or preview is active)
- process audio block (and/or control blocks)
- deactivate (DAW disabled it, playback is inactive and preview is inactive)
- terminate

This is mixture of what LV2 does and what VST3 plugin factory does.

Here is an example:

```
void sample_plugin_delete(
	AndroidAudioPluginFactory *pluginFactory,
	AndroidAudioPlugin *instance)
{
	delete instance;
}

void sample_plugin_prepare(AndroidAudioPlugin *plugin, AndroidAudioPluginBuffer* buffer)
{
	/* do something */
}
void sample_plugin_activate(AndroidAudioPlugin *plugin) {}
void sample_plugin_process(AndroidAudioPlugin *plugin,
	AndroidAudioPluginBuffer* buffer,
	long timeoutInNanoseconds)
{
	/* do something */
}
void sample_plugin_deactivate(AndroidAudioPlugin *plugin) {}

AndroidAudioPluginState state;
const AndroidAudioPluginState* sample_plugin_get_state(AndroidAudioPlugin *plugin)
{
	return &state; /* fill it */
}

void sample_plugin_set_state(AndroidAudioPlugin *plugin, AndroidAudioPluginState *input)
{
	/* apply argument input */
}

AndroidAudioPlugin* sample_plugin_new(
	AndroidAudioPluginFactory *pluginFactory,
	const char* pluginUniqueId,
	int sampleRate,
	const AndroidAudioPluginExtension * const *extensions)
{
	return new AndroidAudioPlugin {
		new SamplePluginSpecific { pluginUniqueId },
		sample_plugin_prepare,
		sample_plugin_activate,
		sample_plugin_process,
		sample_plugin_deactivate,
		sample_plugin_get_state,
		sample_plugin_set_state
		};
}

AndroidAudioPluginFactory* GetAndroidAudioPluginFactory ()
{
	return new AndroidAudioPluginFactory { sample_plugin_new, sample_plugin_delete };
}
```

`GetAndroidAudioPluginFactory` function is the entrypoint, but it will become customizible per plugin, to make it possible to put multiple service bridges (namely LV2 bridge and service bridge) in one shared library.


### out-process model

Unlike in-host-process plugin processing, switching process context is important. Considering the performance loss and limited resources on mobile devices, it is best if we can avoid that. However it is inevitable. It will be handled via [NdkBinder](https://developer.android.com/ndk/reference/group/ndk-binder).

An important note is that NdkBinder API is available only after Android 10 (Android Q). On earlier Android targets the binder must be implemented in Java API. At this state we are not sure if we support compatibility with old targets. They are not great for high audio processing requirement either way.

A Remote plugin client is actually implemented just as a plugin, when it is used by host i.e. the it does not matter how the plugin is implemented.


### fixed pointers

LADSPA and LV2 has somewhat unique characteristics - their port connection is established through raw I/O pointers. Since we support LV2 as one of the backends, the host at least give hint on "initial" pointers to each port, which we can change at run time later but basically only through setting changes (e.g. `connectPort()` for LV2), not at procecss time (e.g. `run()` for LV2). AAP expects plugin developers to deal with dynamically changed pointers at run time. We plugin bridge implementors have no control over host implementations or plugin implementations. So we have to deal with them within the bridged APIs (e.g. call `connectPort()` every time AAP `process()` is invoked with different pointers).

In any case, to pass direct pointers, Android [SharedMemory](https://developer.android.com/ndk/reference/group/memory) (ashmem) should play an important role here. There wouldn't be binary array transmits over binder IPC so far (but if it works better then things might change).

Therefore, at "prepare" step we pass a prepared "buffer" which is supposed to be constructed based on each plugin's ports, so that the plugins do not have to set them up at "process" time.

We are still unsure if this really helps performance (especially how to deal with circular buffers nicely). Things might change.



## AAP package bundle

AAP is packaged as a normal apk. The plugin is implemented in native code, built as a set of shared libraries.

The most important and difficult mission for an audio plugin framework is to get more plugins (hopefully more "quality" plugins, but that is next). Therefore AAP is designed to make existing things reusable. There are some packaging sub-formats e.g. AAP-VST3 or AAP-LV2, to ease plugin developers to reuse existing packaging system.

There is some complexity on how those files are packaged. At the "AAP package helpers" section we describe how things are packaged for each migration pattern.

### Queryable service manifest for plugin lookup

AAP plugins are not managed by Android system. Instead, AAP hosts can query AAPs using PackageManager which can look for specific services by intent filter `org.androidaudiopluginframework.AudioPluginService` and AAP "metadata". Here we follow what Android MIDI API does - AAP developers implement `org.androidaudiopluginframework.AudioPluginService` class and specify it as a `<service>` in `AndroidManifest.xml`. Here is an example

```
<manifest xmlns:android="http://schemas.android.com/apk/res/android"
          package="org.androidaudiopluginframework.samples.aapbarebonesample">

  <uses-permission android:name="android.permission.FOREGROUND_SERVICE" />
  <application>
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
  </application>
</manifest>
```

It should be noted that `AudioPluginService` works as a foreground service, which is required for this kind of audio apps for Android 8.0 or later. Therefore an additional `<uses-permission>` is required for `FOREGROUND_SERVICE`.

The `<service>` element comes up with a `<meta-data>` element. It is to specify an additional XML resource for the service. The `android:resource` attribute indicates that there is `res/xml/aap_metadata.xml` in the project. The file content looks like this:

```
<plugins>
  <plugin manufacturer="AndroidAudioPluginProject"
          name="BareBoneSamplePlugin">
    <ports>
	  <port direction="input" content="midi" name="MidiIn" />
	  <port direction="input" content="other" name="ControlIn" />
	  <port direction="input" content="audio" name="AudioIn" />
	  <port direction="output" content="audio" name="AudioOut" />
    </ports>
  </plugin>
  
  (more <plugin>s...)
</plugins>
```

Only one `<service>` and a metadata XML file is required. A plugin application package can contain more than one plugins (like an LV2 bundle can contain more than one plugins), and they have to be listed on the AAP metadata.

It is a design decision that there is only one service element: then it is possible to host multiple plugins for multiple runners in a single process, which may reduce extra use of resources. JUCE `PluginDescription` also has `hasSharedContainer` field (VST shell supports it).

The metadata format is somewhat hacky for now and subject to change. The metadata content will be similar to what LV2 metadata provides (theirs are in `.ttl` format, ours will remain XML for everyone's consumption and clarity).

AAP hosts can query AAP metadata resources from all the installed app packages, without instantiating those AAP services.

- `<plugin>` element
  - `manufacturer`: name of the plugin manufacturer or developer or whatever.
  - `name`: name of the plugin.
  - `plugin-id`: unique identifier string e.g. `9dc5d529-a0f9-4a69-843f-eb0a5ae44b72`. 
  - `version`: version ID of the plugin.
  - `category`: category of the plugin.
- `<port>` element
  - `name`: a name string. An `xs:NMTOKENS` in XML Schema datatypes is expected.
  - `direction`: either `input` or `output`.
  - `content`: Can be anything, but `audio` and `midi` are recognized by standard AAP hosts.

`name` should be unique enough so that this standalone string can identify itself. An `xs:NMTOKENS` in XML Schema datatypes is expected (not `xs:NMTOKEN` because we accept `#x20`).

`plugin-id` is used by AAP hosts to regard to identify the plugin and expect compatibility e.g. state data and versions, across various environments. This value is used for calculating JUCE `uid` value. Ideally an UUID string, but it's up to the plugin backend. For example, LV2 does identifies each plugin via URI. Therefore we use `lv2:{URI}` when importing from their metadata.

`version` can be displayed by hosts. Desirably it contains build versions or "debug" when developing and/or debugging the plugin, otherwise hosts cannot generate an useful identifier to distinfuish from the hosts.

For `category`, we have undefined format. VST has some strings like `Effect`, `Synth`, `Synth|Instrument`, or `Fx|Delay` when it is detailed. When it contains `Instrument` then it is regarded by the JUCE bridge so far.


## AAP-LV2 backend

### directory structure

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

### LV2_PATH limitation

Although, there is another big limitation on Android platform: it is not possible to get list of asset directories in Android, meaning that querying audio plugins based on the filesystem is not doable. All those plugins must be therefore explicitly listed at some manifest.

To workaround this issue, AAP-LV2 plugin takes a list of LV2 asset paths which are to build `LV2_PATH` settings. It is for both hosts (to load local plugins) and plugins (to provide themselves via server).

### metadata

We decided to NOT support shorthand metadata notation like

```
<plugin backend='LV2' assets='lv2/eg-amp.lv2' product='eg-amp.lv2' />
```

... because it will make metadata non-queryable to normal Android app developers.

Instead we provide a metadata generator tool `app-import-lv2-metadata`:

```
$ ./aap-import-lv2-metadata /sources/LV2/dist/lib/lv2/eg-midigate.lv2 
Loading from /sources/LV2/dist/lib/lv2/eg-midigate.lv2/manifest.ttl
manifest fragment: manifest-fragment.xml
Loaded bundle. Dumping all plugins from there.
Writing metadata file res/xml/metadata0.xml
done.
$ cat res/xml/metadata0.xml 
<plugins>
  <plugin backend="LV2" name="Example MIDI Gate" category="Effect" author="" manufacturer="http://lv2plug.in/ns/lv2" unique-id="lv2:http://lv2plug.in/plugins/eg-midigate">
    <ports>
      <port direction="input" content="midi" name="Control" />
      <port direction="input" content="audio" name="In" />
      <port direction="output" content="audio" name="Out" />
    </ports>
  </plugin>
</plugins>
$ cat manifest-fragment.xml 
<service android:name=".AudioPluginService" android:label="Example MIDI Gate">
  <intent-filter><action android:name="org.androidaudiopluginframework.AudioPluginService" /></intent-filter>
  <meta-data android:name="org.androidaudiopluginframework.AudioPluginService" android:resource="@xml/metadata0" />
</service>
```

For `content`, if a port is `atom:atomPort` and `atom:supports` has `midi:MidiEvent`, then it is `midi`. Any LV2 port that is `lv2:AudioPort` are regarded as `audio`. Anything else is `other` in AAP.

The plugin `category` becomes `Instrument` if and only if it is `lv2:InstrumentPlugin`. Anything else falls back to `Effect`.

We don't detect any impedance mismatch between TTL and metadata XML; LV2 backend implementation uses "lilv" which only loads TTL. lilv doesn't assure port description correctness in TTL either (beyond what lv2validate does as a tool, not runtime).



## AAP-VST3 backend

TBD - there should be similar restriction to that of AAP-LV2.

### blockers

Currently vst3sdk does not build on Android due to:

- GCC dependency on `ext/atomicity.h` and `__gnu_cxx::__atomic_add()`.
  - It is probably replaceable with `std::atomic_fetch_add(reinterpret_cast<std::atomic<int32_t>*> (&var), d)`
- lack of libc++fs (experimental/filesystem) https://github.com/android-ndk/ndk/issues/609
- It is not limited to Android, but `FUID::generate()` is not implemented on Linux (where they could just use `uuid/uuid.h` or `uuid.h` API...)

On a related note, JUCE lacks VST3 support on Linux so far. [They have been working on it](https://forum.juce.com/t/vst3-support-on-linux/31872).



## AAP hosting basics

AAP proof-of-concept host is in `poc-samples/AAPHostSample`.

AAP host will have to support multiple backends e.g. AAP-LV2 and AAP-VST3. LV2 host bridge is implemented using lilv.

Currently AAPHostSample contains *direct* LV2 hosting sample. It will be transformed to AAP hosting application.


### AAP hosting API

It is simpler than LV2. Similar to LV2, ports are connected only by index and no port instance structure for runtime.

Unlike LV2, hosting API is actually used by plugins too, because it has to serve requests from remote host, process audio stream locally, and return the results to the remote host. However plugin developers should not be required to do so by themselves. It should be as easy as implementing plugin framework API and package in AAP format.

- Types - C API
  - `aap::PluginHostSettings`
  - `aap::PluginHost`
  - `aap::PluginHostBackend`
    - `aap::PluginHostBackendLV2`
    - `aap::PluginHostBackendVST3`
  - `aap::PluginInformation`
  - `aap::PortInformation`
  - `aap::PluginInstance`
  - `enum aap::BufferType`
  - `enum aap::PortDirection`

- Functions
  - host
    - `AAPHost* aap_host_create(AAPHostSettings*)`
    - `void aap_host_destroy(AAPHost*)`
    - `AAPPluginDesctiptorIterator* aap_host_get_plugins(AAPHost*)`
    - `AAPInstance* aap_host_instantiate(AAPHost*, AAPDesctiptor*)`
  - plugin desctiptor
    - `AAPPortIterator* aap_descriptor_get_ports(AAPDesctiptor*)`
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


## Samples

### AAPHostSample

It shows existing AAPs (services) as well as in-app plugins that can be loaded in-process.


Behaviors: TODO (For serviecs it connects and disconnects. For local plugins LV2 host runs which takes the input wav sample and generate output wav.

The wave sample is created by atsushieno using Waveform10 and Collective.

(FIXME: the app completely ignores WAV headers so far.)

The waveform rendering is done thanks to audiowave-progressbar: https://github.com/alxrm/audiowave-progressbar



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

### cerbero fork

The external dependencies are built using cerbero build system. Cerbero is a comprehensive build system that cares all standard Android ABIs and builds some complicated projects like glib (which has many dependencies) and cairo.

### LV2 forks

There are couple of lv2 related source repositories, namely serd and lilv. Their common assumption is that they have access to local files, which is not true about Android. They are forked from the original sources and partly rewritten to get working on Android.

And note that access to assets is not as simple as that to filesystem. It is impossible to enumerate assets at runtime. They have to be explicitly named and given. Therefore there are some plugin loader changes in our lilv fork.



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
- tools
  - aap-import-lv2-metadata
