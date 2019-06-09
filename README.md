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

An important note is that NdkBinder API is available only after Android 10 (Android Q). On earlier Android targets the binder must be implemented in Java API. At this state we are not sure if we support compatibility with old targets.

Similarly to LV2, port connection is established as setting raw I/O pointers. Android [SharedMemory](https://developer.android.com/ndk/reference/group/memory) (ashmem) should play an important role here. There wouldn't be binary array transmits over binder IPC so far, but if it works better then things might change.


## AAP package bundle

AAP is packaged as a normal apk. The plugin is implemented in native code, built as a set of shared libraries.

The most important and difficult mission for an audio plugin framework is to get more plugins (hopefully more "quality" plugins, but that is next). Therefore AAP is designed to make existing things reusable. There are some packaging sub-formats e.g. AAP-VST3 or AAP-LV2, to ease plugin developers to reuse existing packaging system.

There is some complexity on how those files are packaged. At the "AAP package helpers" section we describe how things are packaged for each migration pattern.

### Queryable manifest

AAP plugins are not managed at system wide. Instead, AAP hosts can query AAPs using PackageManager which can look for specific services by intent filter `org.androidaudiopluginframework.AndroidAudioPluginService` and AAP "metadata". Here we follow what Android MIDI API does - AAP developers implement `org.androidaudiopluginframework.AndroidAudioPluginService` class and specify it as a `<service>` in `AndroidManifest.xml`. Here is an example

```
<manifest xmlns:android="http://schemas.android.com/apk/res/android"
          package="org.androidaudiopluginframework.samples.aapbarebonesample">
  ...
  <service android:name=".AndroidAudioPluginService"
           android:label="AAPBareBoneSamplePlugin">
    <intent-filter>
      <action 
	    android:name="org.androidaudiopluginframework.AndroidAudioPluginService" />
    </intent-filter>
    <meta-data 
	  android:name="org.androidaudiopluginframework.AndroidAudioPluginService"
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
