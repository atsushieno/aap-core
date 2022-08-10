
# AAP Developers Guide

## What this documentation is for

The most important and difficult mission for an audio plugin framework like AAP is to get more plugins (hopefully more "quality" plugins, but that is next). Therefore AAP is designed to make existing things reusable. We have aap-juce to import JUCE plugins and aap-lv2 to import LV2 plugins to AAP world, to achieve this goal.

Ideally, as long as you have your plugin in either of those plugin structures, you could just import it, as long as the code is designed to work on Android (i.e. not resorting to desktop specifics such as files, or avoid "sync" API such as those in JUCE FileChooser), If you got things working along with these ways, you would't have to understand the AAP architecture described below.

## AAP package bundle

An AAP (plugin) is packaged as an apk. The plugin is implemented in native code, built as a set of shared libraries.

There is some complexity on how those files are packaged. At the "AAP package helpers" section we describe how things are packaged for each migration pattern.

### Queryable service manifest for plugin lookup

Unlike Apple Audio Units, AAP plugins are not managed by Android system. Instead, AAP hosts can query AAPs using PackageManager which can look for specific services by intent filter `org.androidaudioplugin.AudioPluginService.V2` and AAP "metadata". Here we follow what Android MIDI API does - AAP developers implement `org.androidaudioplugin.AudioPluginService` class and specify it as a `<service>` in `AndroidManifest.xml`. Here is an example:

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
	      android:name="org.androidaudioplugin.AudioPluginService.V2" />
      </intent-filter>
      <meta-data 
	    android:name="org.androidaudioplugin.AudioPluginService.V2#Plugins"
	    android:resource="@xml/aap_metadata"
        />
      <meta-data
        android:name="org.androidaudioplugin.AudioPluginService.V2#Extensions"
        android:value="org.androidaudioplugin.lv2.AudioPluginLV2LocalHost"
        />
    </service>
  </application>
</manifest>
```

The `<service>` element comes up with two `<meta-data>` elements.

The simpler one with `org.androidaudioplugin.AudioPluginService.V2#Extensions` is a ',' (comma)-separated list, to specify service-specific "extension" classes. They are loaded via `Class.forName()` and initialized at host startup time with an `android.content.Context` argument. Any AAP service that contains LV2-based plugins has to specify `org.androidaudioplugin.lv2.AudioPluginLV2LocalHost` as the extension. For reference, its `initialize` method looks like:

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

The other one with `org.androidaudioplugin.AudioPluginService.V2#Plugins` is to specify an additional XML resource for the service. The `android:resource` attribute indicates that there is `res/xml/aap_metadata.xml` in the project. The file content looks like this:

```
<plugins xmlns="urn:org.androidaudioplugin.core"
         xmlns:pp="urn:org.androidaudioplugin.port">
  <plugin manufacturer="AndroidAudioPluginProject"
          name="BareBoneSamplePlugin">
    <ports>
	  <port direction="input" content="midi" name="MidiIn" />
	  <port direction="input" content="other" name="ControlIn"
                pp:default="0.5" pp:minimum="0.0" pp:maximum="1.0" />
	  <port direction="input" content="other" name="Enumerated" 
                pp:type="enumeration">
              <pp:enumeration label="11KHz" value="11025" />
              <pp:enumeration label="22KHz" value="22050" />
              <pp:enumeration label="44KHz" value="44100" />
          </port>
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
    - `id` attribute: the port id integer that is supposed to not change as long as parameter compatibility is kept. Indices don't have to be in order.
    - `name` attribute: a name string. An `xs:NMTOKEN` in XML Schema datatypes is expected.
    - `direction` attribute: either `input` or `output`.
    - `content` attribute: Can be anything, but `audio` and `midi` are recognized by standard AAP hosts.
    - `pp:type` attribute: specifies value restriction.
    - `pp:minimum`, `pp:maximum` attributes: specifies value ranges.
    - `pp:default` attribute: specifies the default value.
    - `pp:minimumSize` attribute: specifies the minimum buffer size in bytes.
    - `<pp:enumeration>` element: specifies a candidate value.
      - `label` attribute: value label that is shown to user.
      - `value` attribute: the actual value.

`name` should be unique enough so that this standalone string can identify itself. An `xs:NMTOKENS` in XML Schema datatypes is expected (not `xs:NMTOKEN` because we accept `#x20`).

`plugin-id` is used by AAP hosts to identify the plugin and expect compatibility e.g. state data and versions, across various environments. This value is used for calculating JUCE `uid` value. Ideally an UUID string, but it's up to the plugin backend. For example, LV2 identifies each plugin via URI. Therefore we use `lv2:{URI}` when importing from their metadata.

`version` can be displayed by hosts. Desirably it contains build versions or "debug" when developing and/or debugging the plugin, otherwise hosts cannot generate an useful identifier to distinguish from the hosts.

For `category`, we have undefined format. VST has some strings like `Effect`, `Synth`, `Synth|Instrument`, or `Fx|Delay` when it is detailed. When it contains `Instrument` then it is regarded by the JUCE bridge so far.

`library` is to specify the native shared library name. It is mandatory; if it is skipped, then it points to "androidaudioplugin" which is our internal library which you have no control.

`entrypoint` is to sprcify custom entrypoint function. It is optional; if you simply declared `GetAndroidAudioPluginFactory()` function in the native library, then it is used. Otherwise the function specified by this attribute is used. It is useful if your library has more than one plugin factory entrypoints (like our `libandroidaudioplugin.so` does).

For `pp:type` attribute, currently one of `integer`, `toggled`, `float`, `double` or `enumeration` is expected. It is `float` by default. `enumeration` restricts the value options to the child `<pp:enumeration>` element items.

## AAP Plugin API and implementation

### API references

[Native and Kotlin API references](https://atsushieno.github.io/android-audio-plugin-framework/) are generated by GitHub Actions task.

They are by no means stable and are subject to any changes. For stable development, we recommend to rather depend on LV2 API and use aap-lv2, or JUCE API and ues aap-juce.

There is also an AIDL within the repo. For better backward compatibility we very rarely change it, but it is meant to be totally for internal use only.

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
	AndroidAudioPluginHost *host)
{
	return new AndroidAudioPlugin {
		new SamplePluginSpecific {},
		sample_plugin_prepare,
		sample_plugin_activate,
		sample_plugin_process,
		sample_plugin_deactivate,
		sample_plugin_get_extension
		};
}

AndroidAudioPluginFactory* GetAndroidAudioPluginFactory ()
{
	return new AndroidAudioPluginFactory { sample_plugin_new, sample_plugin_delete, nullptr };
}
```

`GetAndroidAudioPluginFactory` function is the entrypoint, but it will become customizible per plugin (specify it on `aap_metadata.xml`), to make it possible to put multiple service bridges (namely LV2 bridge and service bridge) in one shared library.

## Hosting AAP

We have an AAP proof-of-concept host in `samples/aaphostsample` directory. Note that it is a static audio geenrator, not a live audio processing client.

AAP plugins are queried via `AudioPluginHost.queryAudioPluginServices()` which subsequently issues `PackageManager.queryIntentServices()`, connected using binder. `aaphostsample` issues queries and lists only remote AAPs.

### Important new requirement for Android 11

Android 11 brought in a new restriction on querying information on other applications on the local system. To make service query based on Android intent, we now have to make additional changes to `AndroidManifest.xml` in any AAP host application, to add the extra block below within `<manifest>` element:

```
    <queries>
        <intent>
            <action android:name="org.androidaudioplugin.AudioPluginService.V2" />
        </intent>
    </queries>
```

### (no) need for particular backends

While AAP supports LV2 and probably VST3 in the future, they are not directly included as part of the hosting API and implementation. Instead, they are implemented as plugins. AAP host only processes requests and responses through the internal AAP interfaces. Therefore, there is no need for host "backend" kind of things.

LV2 support is implemented in `androidaudioplugin-lv2` (in [aap-lv2](https://github.com/atsushieno/aap-lv2) repository) as an Android module an AAP plugin. It includes the native library `libandroidaudioplugin-lv2.so` which implements the plugin API using lilv.

VST3 support would be considered similarly, with vst3sdk in mind (but with no actual concrete plan). This (again) does not necessarily mean that *we* provide the bridge.


### AAP Kotlin hosting API

AAP hosting API is exposed to Kotlin to some extent, but it is for kind of compromised feature. Kotlin is not designed to be realtime safe, Android Binder for applications is not RT-safe either, but that's another story, we'd keep trying to make native implementation closer to RT-safe.

At the same time, even with the native API, we still have to resort to Kotlin (JVM) API, as there is no NDK API to query and bind Android Service. Therefore, in some AAP native API, we dynamically query and bind AudioPluginServices. We actually manage client's bound AudioPluginServices using `org.androidaudioplugin.hosting.AudioPluginServiceConnector` and `org.androidaudioplugin.hosting.PluginServiceConnection`.



### AAP native hosting API

It is similar to LV2. Ports are connected only by id (an index-like integer) and no port instance structure for runtime buffers.

Unlike LV2, hosting API is actually used by plugins too, because it has to serve requests from remote host, process audio stream locally, and return the results to the remote host. But plugin developers shouldn't have to worry about it. It should be as easy as implementing plugin API and package in AAP format.

Currently, the Hosting API is provided only in C++. They are in `aap` namespace.

- Types
  - `class PluginHost`
    - `class PluginService` for service implementation (not really intended for public use)
    - `class PluginClient` for hosting implementation
  - `class PluginClientSystem`
    - `class AndroidPluginClientSystem`
    - `class DesktopPluginClientSystem` (it does not exist yet)
  - `class PluginInformation`
  - `class PortInformation`
  - `class PluginInstance`
    - `class LocalPluginInstance` for service's plugin instances
    - `class RemotePluginInstance` for client's plugin instances
  - `enum aap::ContentType`
  - `enum aap::PortDirection`
  - `enum aap::PluginInstantiationState`

The API is still not really well-thought at this state and subject to any changes.

### Accessing Remote plugin

Plugin developers don't really have to deal with hosting. The only connected point would be extensions, which is explained in [`EXTENSIONS.md`](./EXTENSIONS.md).

Host developers would use `PluginClient` class to instantiate plugins from various (one or more) AudioPluginServices. It requires live connection list instance (`PluginClientConnectionList*`) and a snapshot of plugin list (`PluginListSnapshot*`), which require some JNI interaction on Android.

`PluginListSnapshot` is easy to acquire - there is `PluginListSnapshot::queryServices()` that dispatches query to Intent Service via Android (Java) API and get results. `PluginClientConnectionList` is a little bit more complicated - you can create new `PluginClientConnection` object with NdkBinder, which can be acquired only through `android.content.Context.bindService()` i.e. via Android (Java) API (again).

Plugins that have the bound service in `PluginClientConnectionList` can be instantiated in synchronous API `PluginClient::createInstance()`. For others, asynchronous `PluginClient::createInstanceAsync()` has to be used. It dispatches instantiation process to `PluginClientSystem` to dynamically bind service. For Android it is `AndroidPluginClientSystem`which simply runs `Context.bindService()`. (There could be desktop version of the API feature parity, which is not implemented yet.)

Those create~ methods only returns the instance ID, but you can easily get the actual `aap::RemotePluginInstance` instance via `getInstance()`. This instance lets you do the rest of the plugin processing (prepare/activate/process/deactivate).

There may be further changes to the API around there, but so far that's how things would work.

### native PluginClient instancing internals

(TODO: this section should probably go into HACKING.md)

This section describes some internals about `aap::PluginClient::createInstanceAsync()`. Plugin host developers can just use this function.

Remote plugins can be accessed through AAP "client bridge" which is publicly just an AAP plugin (so that general AAP Hosts can instantiate locally), while it implements the API as a Service client, using NdkBinder API.

Each AAP resides in an application, and it works as an Android Service, named ` org.androidaudioplugin.AudioPluginService`. There is an AIDL which somewhat similar to the AAP plugin API (but with an instance ID, and has more callable methods):

```
package org.androidaudioplugin;

interface AudioPluginInterface {

	int beginCreate(String pluginId, int sampleRate);
	void addExtension(int instanceID, String uri, in ParcelFileDescriptor sharedMemoryFD, int size);
	void endCreate(int instanceID);

	boolean isPluginAlive(int instanceID);

	void prepare(int instanceID, int frameCount, int portCount);
	void prepareMemory(int instanceID, int shmFDIndex, in ParcelFileDescriptor sharedMemoryFD);
	void activate(int instanceID);
	void process(int instanceID, int timeoutInNanoseconds);
	void deactivate(int instanceID);
	
	void destroy(int instanceID);
}

```

Due to [AIDL tool limitation or framework limitation](https://issuetracker.google.com/issues/144204660), we cannot use `List<ParcelFileDescriptor>`, therefore `prepareMemory()` is added apart from `prepare()` to workaround this issue.
