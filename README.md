# AAP: Android Audio Plugin Framework

Build Status: [![Build Status](https://app.bitrise.io/app/d9d69ce50556d890/status.svg?token=e_wmxFsLRDnqgNfuYa6cfg)](https://app.bitrise.io/app/d9d69ce50556d890)

disclaimer: the README is either up to date, partially obsoleted, or sometimes ahead of implementation (to describe the ideal state). Do not trust it too much.


## What is AAP?

Android lacks commonly used Audio Plugin Framework. On Windows and other desktops, VSTs are popular. On Mac and iOS (including iPadOS) there is AudioUnit. On Linux LV2 is used, to some extent.

There is no such thing in Android. Android Audio Plugin (AAP) Framework is to fill this gap.

What AAP aims is to become like an inclusive standard for audio plugin, adoped to Android applications ecosystem. The license is permissive (MIT). It is designed to be pluggable from other specific audio plugin specifications like VST3, LV2, DISTRHO, CLAP, and so on (not necessarily meant that *we* write code for them).

On the other hand it is designed so that cross-audio-plugin frameworks can support it. We have [JUCE](http://juce.com/) integration support, and once [iPlug2](https://iplug2.github.io/) supports Linux and Android it would become similarly possible. Namely, AAP is first designed so that JUCE audio processor can be implemented and JUCE-based audio plugins can be easily imported to AAP world.

Extensibility is provided like what LV2 does (but without RDF and Turtle complication). VST3-specifics, or AAX-specifics, can be represented as long as it can be represented through raw pointer of any type (`void*`) i.e. cast to any context you'd like to have, associalted with a URI. Those extensions can be used only with supported hosts. A host can query each plugin whether it supports certain feature or not and disable those not-supported plugins, and a plugin can query the host which features it provides.

Android is the first citizen in AAP, but we also support Linux desktop so that actual plugin development can be achieved on the desktop.

There are examples based on LV2 plugins, including [mda-lv2](https://gitlab.com/drobilla/mda-lv2) (which was ported from [mda-vst](https://sourceforge.net/projects/mda-vst/)), as well as porting helper utility to generate XML metadata from LV2 plugins. Also, we already have JUCE audio processors for AAP (`AndroidAudioPluginFormat`) in [aap-juce](https://github.com/atsushieno/aap-juce) repository.


## How AAPs work: technical background

AAP distribution structure is simple; both hosts (DAWs) and plugins (instruments/effects) can be shipped as Android applications (via Google Play etc.) respectively:

- AAP Host (DAW) developers can use AAP hosting API to query and load the plugins, then deal with audio data processed by them.
- AAP (Plugin) developers works as a service, processes audio and MIDI messages.

From app packagers perspective and users perspective, it can be distributed like a MIDI device service. Like Android Native MIDI (introduced in Android 10.0), AAP processes all the audio stuff in native land (it still performs metadata queries and service queries in Dalvik/ART land).

AAP developers create audio plugin in native code using Android NDK, create plugin "metadata" as an Android XML resource (`aap_metadata.xml`), and optionally implement `org.androidaudioplugin.AudioPluginService` which handles audio plugin connections using Android SDK, then package them together. The metadata provides developer details, port details (as long as they are known), and feature requirement details.

TODO: The plugins and their ports can NOT be dynamically changed, at least as of the current specification stage. We should seriously reconsider this. It will be mandatory when we support so-called plugin wrappers and loadable instrumental banks (e.g. [dexed](https://github.com/asb2m10/dexed) Carts).

AAP is similar to what [AudioRoute](https://audioroute.ntrack.com/developer-guide.php) hosted apps do. We are rather native oriented for reusing existing code. (I believe they also aim to implement plugins in native code, but not sure. The SDK is not frequently updated.)


### out-process model

Unlike in-host-process plugin processing, switching process context is important. Considering the performance loss and limited resources on mobile devices, it is best if we can avoid that. However it is inevitable. It will be handled via [NdkBinder](https://developer.android.com/ndk/reference/group/ndk-binder).

An important note is that NdkBinder API is available only after Android 10. On earlier Android targets the binder must be implemented in Java API (or through non-public native API, which is now prohibited). At this state we are not sure if we support compatibility with old devices. They are not great for high audio processing requirement either way.

A Remote plugin client is actually implemented just as a plugin, when it is used by host i.e. it does not matter how the plugin is implemented. Though at this state we are not sure if we provide direct native client API. As of writing this, there is no way to bind services (`AudioPluginService`s) using ndkbinder API. So far we use Java binder API instead.


### Shared memory and fixed pointers

LADSPA and LV2 has somewhat unique characteristics - their port connection is established through raw I/O pointers in prior to processing buffers. Since we support LV2 as one of the backends, the host at least gives hint on "initial" pointers to each port, which we can change at run time later but basically only by setting changes (e.g. `connectPort()` for LV2), not at procecss time (e.g. `run()` for LV2). AAP expects plugin developers to deal with dynamically changed pointers at run time. We plugin bridge implementors have no control over host implementations or plugin implementations. So we have to deal with them within the bridged APIs (e.g. call `connectPort()` every time AAP `process()` is invoked with different pointers).

In any case, to pass direct pointers, Android [SharedMemory](https://developer.android.com/ndk/reference/group/memory) plays an important role here. There wouldn't be binary array transmits over binder IPC so far. This is also what AudioRoute does too.

Therefore, at "prepare" step we pass a prepared "buffer" which is supposed to be constructed based on each plugin's ports, so that the plugins do not have to set them up at "process" time.

We are still unsure if this really helps performance (especially how to deal with circular buffers nicely). Things might change.

(A related annoyance is that when targeting API Level 29 direct access to `/dev/ashmem` is prohibited and use of `ASharedMemory` API is required, meaning that it's going to be Android-only codebase. Making it common to desktop require code abstraction for ashmem access, which sounds absurd.)


### Realtime expectation: AAudio and realtime binder

AAP expects some new Android platform features, namely:

- sharing audio plugin buffers through ASharedMemory (ashmem) (Android 4.0.3)
- realtime audio processing through AAudio/Oboe (Android 8.1 in practice)
- AudioPluginService depends on NdkBinder (Android 10.0)
- realtime IPC binder (Android 8.0)

The last one is most likely important, if we process audio data through Oboe callback (callbacks can be handled via plain AAudio API, but Oboe is likely to provide more features as well as being used by other frameworks like JUCE).



## AAP Plugin API and implementation

### plugin API

From each audio plugin's point of view, it is locally instantiated by each service application. Here is a brief workflow for a plugin from the beginning, through processing audio and MIDI inputs (or any kind of controls), to the end:

- get plugin factory
- instantiate plugin (pass plugin ID and sampleRate)
- prepare (pass initial buffer pointers)
- activate (DAW enabled it, playback is active or preview is active)
- process audio block (and/or control blocks)
- deactivate (DAW disabled it, playback is inactive and preview is inactive)
- terminate and destroy the plugin instance

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
void sample_plugin_get_state(AndroidAudioPlugin *plugin, AndroidAudioPluginState* result)
{
	/* fill state data into result */
}

void sample_plugin_set_state(AndroidAudioPlugin *plugin, AndroidAudioPluginState *input)
{
	/* apply argument input state to the plugin */
}

typedef struct {
    /* any kind of extension information, which will be passed as void* */
} SamplePluginSpecific;

AndroidAudioPlugin* sample_plugin_new(
	AndroidAudioPluginFactory *pluginFactory,
	const char* pluginUniqueId,
	int sampleRate,
	const AndroidAudioPluginExtension * const *extensions)
{
	return new AndroidAudioPlugin {
		new SamplePluginSpecific {},
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

`GetAndroidAudioPluginFactory` function is the entrypoint, but it will become customizible per plugin (specify it on `aap_metadata.xml`), to make it possible to put multiple service bridges (namely LV2 bridge and service bridge) in one shared library.


### AAP package bundle

AAP is packaged as a normal apk. The plugin is implemented in native code, built as a set of shared libraries.

The most important and difficult mission for an audio plugin framework is to get more plugins (hopefully more "quality" plugins, but that is next). Therefore AAP is designed to make existing things reusable. There are some packaging sub-formats e.g. AAP-VST3 or AAP-LV2, to ease plugin developers to reuse existing packaging system.

There is some complexity on how those files are packaged. At the "AAP package helpers" section we describe how things are packaged for each migration pattern.


### Queryable service manifest for plugin lookup

Unlike Apple Audio Units, AAP plugins are not managed by Android system. Instead, AAP hosts can query AAPs using PackageManager which can look for specific services by intent filter `org.androidaudioplugin.AudioPluginService` and AAP "metadata". Here we follow what Android MIDI API does - AAP developers implement `org.androidaudioplugin.AudioPluginService` class and specify it as a `<service>` in `AndroidManifest.xml`. Here is an example:

```
<manifest xmlns:android="http://schemas.android.com/apk/res/android"
          package="org.androidaudioplugin.samples.aapbarebonesample">

  <uses-permission android:name="android.permission.FOREGROUND_SERVICE" />
  <application>
    ...
    <service android:name=".AudioPluginService"
             android:label="AAPBareBoneSamplePlugin">
      <intent-filter>
        <action 
	      android:name="org.androidaudioplugin.AudioPluginService" />
      </intent-filter>
      <meta-data 
	    android:name="org.androidaudioplugin.AudioPluginService#Plugins"
	    android:resource="@xml/aap_metadata"
        />
      <meta-data
        android:name="org.androidaudioplugin.AudioPluginService#Extensions"
        android:value="org.androidaudioplugin.lv2.AudioPluginLV2LocalHost"
        />
    </service>
  </application>
</manifest>
```

It should be noted that `AudioPluginService` works as a foreground service, which is required for this kind of audio apps for Android 8.0 or later. Therefore an additional `<uses-permission>` is required for `FOREGROUND_SERVICE`.

The `<service>` element comes up with two `<meta-data>` elements.

The simpler one with `org.androidaudioplugin.AudioPluginService#Extensions` is a ',' (comma)-separated list, to specify service-specific "extension" classes. They are loaded via `Class.forName()` and initialized at host startup time with an `android.content.Context` argument. Any AAP service that contains LV2-based plugins has to specify `org.androidaudioplugin.lv2.AudioPluginLV2LocalHost` as the extension. For reference, its `initialize` method looks like:

```
@JvmStatic
fun initialize(context: Context)
{
	var lv2Paths = AudioPluginHost.getLocalAudioPluginService(context).plugins
		.filter { p -> p.backend == "LV2" }.map { p -> if(p.assets != null) p.assets!! else "" }
		.distinct().toTypedArray()
	initialize(lv2Paths.joinToString(":"), context.assets)
}

@JvmStatic
external fun initialize(lv2Path: String, assets: AssetManager)
```

The other one with `org.androidaudioplugin.AudioPluginService#Plugins` is to specify an additional XML resource for the service. The `android:resource` attribute indicates that there is `res/xml/aap_metadata.xml` in the project. The file content looks like this:

```
<plugins>
  <plugin manufacturer="AndroidAudioPluginProject"
          name="BareBoneSamplePlugin">
    <ports>
	  <port direction="input" content="midi" name="MidiIn" />
	  <port direction="input" default="0.5" minimum="0.0" maximum="1.0" content="other" name="ControlIn" />
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
  - `manufacturer` attribute: name of the plugin manufacturer or developer or whatever.
  - `name` attribute: name of the plugin.
  - `plugin-id` attribute: unique identifier string e.g. `9dc5d529-a0f9-4a69-843f-eb0a5ae44b72`. 
  - `version` attribute: version ID of the plugin.
  - `category` attribute: category of the plugin.
  - `library` attribute: native library file name that contains the plugin entrypoint
  - `entrypoint` attribute: name of the entrypoint function name in the library. If it is not specified, then `GetAndroidAudioPluginFactory` is used.
  - `assets` attribute: an asset directory which contains related resources. It is optional. Most of the plugins would contain additional resources though.
- `<ports>` element - defines port group (can be nested)
  - `name`: attribute: port group name. An `xs:NMTOKEN` in XML Schema datatypes is expected.
  - `<port>` element
    - `name` attribute: a name string. An `xs:NMTOKEN` in XML Schema datatypes is expected.
    - `direction` attribute: either `input` or `output`.
    - `content` attribute: Can be anything, but `audio` and `midi` are recognized by standard AAP hosts.

`name` should be unique enough so that this standalone string can identify itself. An `xs:NMTOKENS` in XML Schema datatypes is expected (not `xs:NMTOKEN` because we accept `#x20`).

`plugin-id` is used by AAP hosts to identify the plugin and expect compatibility e.g. state data and versions, across various environments. This value is used for calculating JUCE `uid` value. Ideally an UUID string, but it's up to the plugin backend. For example, LV2 identifies each plugin via URI. Therefore we use `lv2:{URI}` when importing from their metadata.

`version` can be displayed by hosts. Desirably it contains build versions or "debug" when developing and/or debugging the plugin, otherwise hosts cannot generate an useful identifier to distinguish from the hosts.

For `category`, we have undefined format. VST has some strings like `Effect`, `Synth`, `Synth|Instrument`, or `Fx|Delay` when it is detailed. When it contains `Instrument` then it is regarded by the JUCE bridge so far.

`library` is to specify the native shared library name. It is mandatory; if it is skipped, then it points to "androidaudioplugin" which is our internal library which you have no control.

`entrypoint` is to sprcify custom entrypoint function. It is optional; if you simply declared `GetAndroidAudioPluginFactory()` function in the native library, then it is used. Otherwise the function specified by this attribute is used. It is useful if your library has more than one plugin factory entrypoints (like our `libandroidaudioplugin.so` does).


### MIDI input support

MIDI ports are different from audio ports and control ports in some ways. Audio and control data ports receive contiguous "raw" data which are direct output values to the destination or input values from the source, but MIDI inputs are from events with specific duration per event, or immediate direct input. On the other hand, they usually have to be processed in sync with audio inputs because (1) those MIDI inputs can be controllers (think about audio effects) and (2) audio processing should be highly efficient so that the number of processing requests must be kept as minimum as possible. Therefore usually audio plugin frameworks process audio and MIDI data at a time, and we follow that.

The approach to process MIDI data is similar between audio plugin frameworks, but the actual content format is different for each framework. Usually they come up with structured message format (VST/AU/LV2 Atom), but AAP so far expects a "length-prepended" SMF track-like inputs which are just sequence of pairs of delta time and MIDI event for "MIDI" channel. AAP does not limit the format to audio and MIDI - the port content can be anything. It is up to agreement between plugins and hosts. The MIDI data length is provided as size in bytes in 32-bit signed integer.

(A related note: to make direct inputs from MIDI devices not laggy, the audio processing buffer should be kept as minimum as possible when dealing with MIDI inputs. That in general results in high CPU (and thus power) usage, which is usually problematic to mobile devices. There should be some consideration on power usage on each application.)

The first specification of the MIDI buffer format is as follows:

- First 4 bytes: int32_t for time division specifier, which is the same as the division specifier in SMF MIDI header chunk.
- Next 4 bytes: the length of rest of MIDI buffer in bytes.
- the rest of the bytes: list of MIDI messages, where each of MIDI message is a pair of length and MIDI event.

Note: JUCE sends timestamp values as application-dependent, meaning that something must be externally spdcifyable to host and they have to be accessible from each audio plugin i.e. through some API.



## Importing LV2 plugins

### directory structure conversion

LV2 packaging is not straightforward. Android native libraries are usually packaged like

- `lib/armeabi-v7a/libfoo.so`
- `lib/arm64-v8a/libfoo.so`
- `lib/x86/libfoo.so`
- `lib/x86_64/libfoo.so`

while normal `lv2` packages usually look like:

- `lib/foo.lv2/manifest.ttl`
- `lib/foo.lv2/foo.ttl`
- `lib/foo.lv2/foo.so`

Attempt to copy those `lv2` contents under `lib/{abi}` with simple build.gradle script failed. Asking plugin developers to add `copy(from/into)` operation hack (which might still not work) is awkward, so we would rather provide simpler solution - we put `lv2/` contents under `assets`, and put ABI-specific `*.so` files directly under `lib/{abi}`. Loading `*.so` from `assets` subdirectories is not possible either.

- `assets/foo.lv2/manifest.ttl`
- `assets/foo.lv2/foo.ttl`
- `lib/{abi}/foo.so`

The `import-lv2-deps.sh` does this task for `java/aaplv2samples` which is used by `aaphostsample` and `localpluginsample`.


### LV2_PATH workarounds

There is another big limitation on Android platform: it is not possible to get list of asset directories in Android, meaning that querying audio plugins based on the filesystem is not doable. All those plugins must be therefore explicitly listed at some manifest.

To address this issue, AAP-LV2 plugin service takes a list of LV2 asset paths from `aap_metadata.xml`, which are used for `LV2_PATH` settings. It is taken care by `org.androidaudioplugin.lv2.AudioPluginLV2ServiceExtension` and plugin developers shouldn't have to worry about it, as long as they add the following metadata within `<service>` for AudioPluginService:

```
            <meta-data
                android:name="org.androidaudioplugin.AudioPluginService#Extensions"
                android:value="org.androidaudioplugin.lv2.AudioPluginLV2ServiceExtension"
                />
```


### converting LV2 metadata to AAP metadata

We decided to NOT support shorthand metadata notation like

```
<plugin backend='LV2' assets='lv2/eg-amp.lv2' product='eg-amp.lv2' />
```

... because it will make metadata non-queryable to normal Android app developers.

Instead we provide a metadata generator tool `app-import-lv2-metadata` and ask LV2 plugin developers (importers) to describe everything in `aap-metadata.xml`:

```
$ ./aap-import-lv2-metadata [lv2path] [res_xml_path]
(...)
LV2 directory: /sources/android-audio-plugin-framework/java/samples/aaphostsample/src/main/assets/lv2
Loading from /sources/android-audio-plugin-framework/java/samples/aaphostsample/src/main/assets/lv2/ui.lv2/manifest.ttl
Loaded bundle. Dumping all plugins from there.
all plugins loaded
Writing metadata file java/samples/aaphostsample/src/main/res/xml/aap_metadata.xml
done.

$ cat res/xml/metadata0.xml 
<plugins>
  <plugin backend="LV2" name="Example MIDI Gate" category="Effect" author="" manufacturer="http://lv2plug.in/ns/lv2" unique-id="lv2:http://lv2plug.in/plugins/eg-midigate" library="..." entrypoint="...">
    <ports>
      <port direction="input" content="midi" name="Control" />
      <port direction="input" content="audio" name="In" />
      <port direction="output" content="audio" name="Out" />
    </ports>
  </plugin>
</plugins>
$ cat manifest-fragment.xml 
<plugins>
  <plugin backend="LV2" name="MDA Ambience" category="Effect" author="David Robillard" manufacturer="http://drobilla.net/plugins/mda/" unique-id="lv2:http://drobilla.net/plugins/mda/Ambience" library="libandroidaudioplugin-lv2.so" entrypoint="GetAndroidAudioPluginFactoryLV2Bridge" assets="/lv2/mda.lv2/">
    <ports>
      <port direction="input" default="0.700000" minimum="0.000000" maximum="1.000000" content="other" name="Size" />
      <port direction="input" default="0.700000" minimum="0.000000" maximum="1.000000" content="other" name="HF Damp" />
      <port direction="input" default="0.900000" minimum="0.000000" maximum="1.000000" content="other" name="Mix" />
      <port direction="input" default="0.500000" minimum="0.000000" maximum="1.000000" content="other" name="Output" />
      <port direction="input"    content="audio" name="Left In" />
      <port direction="input"    content="audio" name="Right In" />
      <port direction="output"    content="audio" name="Left Out" />
      <port direction="output"    content="audio" name="Right Out" />
    </ports>
  </plugin>
  ...
```

For `content`, if a port is `atom:atomPort` and `atom:supports` has `midi:MidiEvent`, then it is `midi`. Any LV2 port that is `lv2:AudioPort` are regarded as `audio`. Anything else is `other` in AAP.

The plugin `category` becomes `Instrument` if and only if it is `lv2:InstrumentPlugin`. Anything else falls back to `Effect`.

We don't detect any impedance mismatch between TTL and metadata XML; LV2 backend implementation uses "lilv" which only loads TTL. lilv doesn't assure port description correctness in TTL either (beyond what lv2validate does as a tool, not runtime).


### Licensing notice

Note that `mda-lv2` is distributed under the GPLv3 license and you have to
follow it when distributing or making changes to that part (the LV2 plugin
samples). This does not apply to other LV2 related bits.



## Importing VST3 plugins

TBD - there should be similar restriction to that of AAP-LV2.

### blockers

Currently vst3sdk does not build on Android due to:

- GCC dependency on `ext/atomicity.h` and `__gnu_cxx::__atomic_add()`.
  - It is probably replaceable with `std::atomic_fetch_add(reinterpret_cast<std::atomic<int32_t>*> (&var), d)`
- lack of libc++fs (experimental/filesystem) https://github.com/android-ndk/ndk/issues/609 **NOTE**: it looks as if its milestone is set for NDK r21, but it had been marked since back to r16, so do not trust the milestone.
- It is not limited to Android, but `FUID::generate()` is not implemented on Linux (where they could just use `uuid/uuid.h` or `uuid.h` API...)

On a related note, JUCE lacks VST3 support on Linux so far. [They have been working on it](https://forum.juce.com/t/vst3-support-on-linux/31872) for more than a year.



## Hosting AAP

We have AAP native API documentation hosted at: https://androidaudioplugin.web.app/references/native/ . It is not strictly in sync with the repository, let us know if you noticed that it is not up to date enough. Also right not it only provides the API structure without any documentation comments.

We have an AAP proof-of-concept host in `java/samples/aaphostsample` directory.

AAP plugins are queried via `AudioPluginHost.queryAudioPluginServices()` which subsequently issues `PackageManager.queryIntentServices()`, connected using binder. `aaphostsample` issues queries and lists only remote AAPs.

There is another hosting sample in `java/samples/localpluginsample`, which does not support remote plugins. Instead it only lists local AAPs.


### (no) need for particular backends

While AAP supports LV2 and probably VST3 in the future, they are not directly included as part of the hosting API and implementation. Instead, they are implemented as plugins. AAP host only processes requests and responses through the internal AAP interfaces. Therefore, there is no need for host "backend" kind of things.

LV2 support is implemented in `androidaudioplugin-lv2` Android module with native library `libandroidaudioplugin-lv2.so` which itself is an AAP plugin. It implements the plugin API using lilv. VST3 support is planned similarly, with vst3sdk in mind.


### AAP native hosting API

It is similar to LV2. Ports are connected only by index and no port instance structure for runtime.

Unlike LV2, hosting API is actually used by plugins too, because it has to serve requests from remote host, process audio stream locally, and return the results to the remote host. However plugin developers should not be required to do so by themselves. It should be as easy as implementing plugin framework API and package in AAP format.

TODO: there should be API reference generated by doxygen.

Currently, hosting API is provided only in C++. They are in `aap` namespace.

- Types
  - `class PluginHost`
  - `class PluginHostPAL`
    - `class AndroidPluginHostPAL`
    - `class DesktopPluginHostPAL`
  - `class PluginInformation`
  - `class PortInformation`
  - `class PluginInstance`
  - `enum aap::ContentType`
  - `enum aap::PortDirection`
  - `enum aap::PluginInstantiationState`

The API is not really well-thought at this state and subject to any changes.


### Accessing Remote plugin

This section describes some internals about `aap::PluginHost::instantiatePlugin()`. Plugin host developers can just use this function.

Remote plugins can be accessed through AAP "client bridge" which is publicly just an AAP plugin (so that general AAP Hosts can instantiate locally), while it implements the API as a Service client, using NdkBinder API.

Each AAP resides in an application, and it works as an Android Service, named ` org.androidaudioplugin.AudioPluginService`. There is an AIDL which somewhat similar to the AAP plugin API (but with an instance ID, and has more callable methods):

```
package org.androidaudioplugin;

interface AudioPluginInterface {

	int create(String pluginId, int sampleRate);

	boolean isPluginAlive(int instanceID);

	void prepare(int instanceID, int frameCount, int portCount);
	void prepareMemory(int instanceID, int shmFDIndex, in ParcelFileDescriptor sharedMemoryFD);
	void activate(int instanceID);
	void process(int instanceID, int timeoutInNanoseconds);
	void deactivate(int instanceID);
	int getStateSize(int instanceID);
	void getState(int instanceID, in ParcelFileDescriptor sharedMemoryFD);
	void setState(int instanceID, in ParcelFileDescriptor sharedMemoryFD, int size);
	
	void destroy(int instanceID);
}

```

Due to [AIDL tool limitation or framework limitation](https://issuetracker.google.com/issues/144204660), we cannot use `List<ParcelFileDescriptor>`, therefore `prepareMemory()` is added apart from `prepare()` to workaround this issue.



## Desktop support

AAP is a framework which is also intended to be cross platform on Unix and optionally other OSes. Although there is no Android service and binder on those OSes and therefore we need alternatives.

It is up to hosting application, but there is a normative way to look up AAPs on the system. Unlike Android, they don't have to exist as services. Although the AAP manifest takes place to provide plugin listing. It contains `library` and `entrypoint` that can be simply reused on other platforms.

For a normative AAP host, `AAP_PLUGIN_PATH` environment variable is used to look up AAPs on the specified paths. The environment variable contains the list of paths which are joined by platform-specific separator. Here is the list of well-known `AAP_PLUGIN_PATH` values per platform:

- Linux and other Unix variants: `$HOME/.aap:/usr/local/lib/aap:/usr/lib/aap`
- MacOS (not sure if it will be supported): `$HOME/Library/Audio/Plug-ins/AAP:/Library/Audio/Plug-ins/AAP`
- Windows (not sure if it will be supported): `c:\AAP Plugins;c:\Program Files[\AAP Plugins`
  - There is no plan to support 32bit builds on 64-bit OS. Unlike VSTs we have no 32bit legacy.



## JUCE integration

The development has moved to [aap-juce](https://github.com/atsushieno/aap-juce).


## Sample apps

### aaphostsample and localpluginsample

`aaphostsample` shows existing AAPs (services) that are installed on the same device. `localpluginsample` bundles those AAPs within the application itself and thus avoids remote instancing.

Local hosting would be useful for whoever wants to complete audio application within its package itself i.e. whoever just want to reuse existing LV2 plugins without any extensibility.

The wave sample is created by atsushieno using Waveform10 and Collective.

As of writing this, the app does not respect any audio format and processes in fixed size, and sends some fixed MIDI messages (note on/off-s) to AAP plugins.

The waveform rendering is done thanks to [waveformSeekBar](https://github.com/massoudss/waveformSeekBar).



## Building repo

Basically `make` on Unix-y environment will build every native dependencies and Kotlin based samples. You will have to specify `ANDROID_SDK_ROOT`.

Android NDK will be downloaded to `~/Android/Sdk/ndk/{rev}` unless `ANDROID_NDK` variable is externally specified to `make`.

CMake is used as **part of** the entire builds, for building native part of the code, **once for one ABI**. `make`, on the other hand, will build the native libraries for all supported ABIs.

gradle is used as **part of** the entire builds, for Android application samples and libraries.

Once native dependencies are set up, Android Studio can be used for development by opening `java` directory.


## Build Dependencies

### Platform features and modules

TODO: move those descriptions to `android-native-audio-builders` repo.

android-audio-plugin-framework repo has some dependencies, which are either platform-level-specific, or external. Note that this is NOT about build script.

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
- vst3 category (TODO)
  - vst3sdk (no particular dependency found, for non-GUI parts)

### cerbero fork

The external dependencies are built using cerbero build system. Cerbero is a comprehensive build system that cares all standard Android ABIs and builds some complicated projects like glib (which has many dependencies) and cairo.


### LV2 forks

There are couple of lv2 related source repositories, namely serd and lilv. Their common assumption is that they have access to local files, which is not true about Android. They are forked from the original sources and partly rewritten to get working on Android.

And note that access to assets is not as simple as that to filesystem. It is impossible to enumerate assets at runtime. They have to be explicitly named and given. Therefore there are some plugin loader changes in our lilv fork.


### android-audio-plugin-framework source tree structure

- README.md - this file.
- aidl - contains common internal interface definitions for native and Kotlin modules.
- dependencies - native source dependencies. Most of them are now part of [android-native-audio-builders](https://github.com/atsushieno/android-native-audio-builders/).
- native
  - plugin-api (C include file; it is just for reference for plugin developers)
  - core
    - include - AAP C++ header files for native host developers.
      - (plugins don't have to reference anything. Packaging is another story though.)
    - src - AAP hosting reference implementation
  - android (Android-specific parts; NdkBinder etc.)
  - desktop (general Unix-y desktop specific parts; POSIX-IPC maybe)
  - lv2 (LV2-specific parts)
  - vst3 (VST3-specific parts; TODO)
- java
  - androidaudioplugin (aar)
    - lib
    - test (currently empty)
    - androidTest (currently empty)
  - androidaudioplugin-LV2 (aar) - follows the same AGP-modules structure
  - androidaudioplugin-VST3 (aar; TODO) - follows the same AGP-modules structure
  - samples
    - aaphostsample - sample host app - follows the same AGP-modules structure
    - aaplv2plugins - library module (aar) that contains LV2 plugins (resources and manifests)
    - aappluginsample - sample LV2 plugins to be consumed by out of process hosts
    - localpluginsample - sample host and plugins bundled within an application
- tools
  - aap-import-lv2-metadata


### Running host app and plugins app

AAP is kind of client-server model, and to fully test AAP framework there should be two apps (for a client and a server). Therefore, there are `aaphostsample` and `aappluginsample` for completeness. Both have to be installed to try out.

There is another sample `localpluginsample` which does not really demonstrate inter-app connection but demonstrates local plugin processing instead. Note that those plugins in the app is also queryabled and indeed shows up when `aaphostsample` is run (and if this `localpluginsample` in installed).

On Android Studio, you can switch which app to launch.


### Debugging Audio Plugins (Services)

When you would like to debug your AudioPluginServices, an important thing to note is that services (plugins) are not debuggable until you invoke `android.os.Debug.waitForDebugger()`, and you cannot invoke this method when you are NOT debugging (it will wait forever!) as long as the host and client are different apps. This also applies to native debugging via lldb (to my understanding).

`aappluginsample` therefore invokes this method only at `MainAcivity.onCreate()` i,e. it is kind of obvious that it is being debugged.


### Hacking on desktop

android-audio-plugin-framework is (against the name) designed to be kind of cross-platform so that it is hackable on desktop too. It is often much easier if we can diagnose the issues on desktop natively, without resorting to remote debugging on Android targets.

In `Makefile`, those LV2 dependencies on desktop are built with debugging symbols by default (`waf -d`).

Debugging will become easier by setting the following environment variables (replace `{.../android-audio-plugin-framework}` part to match your repo checkout), as the local checkouts including submodules would become the debuggee sources:

- `AAP_PLUGIN_PATH={.../android-audio-plugin-framework}/java/samples/aaplv2plugins/src/main/res/xml`
- `LD_LIBRARY_PATH={.../android-audio-plugin-framework}/build/native/androidaudioplugin-lv2/:{.../android-audio-plugin-framework}/dependencies/lv2-desktop/dist/lib/:$LD_LIBRARY_PATH`
- `LV2_PATH={.../android-audio-plugin-framework}/dependencies/lv2-desktop/dist/lib/lv2/`


### Build with AddressSanitizer

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

