# AAP Extension Developer Guide

!!! DRAFT DRAFT DRAFT !!!

It is so far a design draft documentation that does NOT describe what we already have.

## What Extensibility means for AAP

AAP is designed to be extensible, which means, anyone can define features that can be used by both host and client. Lots of AAP features are implemented as AAP Extensions. Plugin extensbility is conceptually similar to VST-MA, LV2, and CLAP Extensions.

On the other hand, AAP is unique in that it has to work **across separate processes** (host process and plugin processes), which makes extensibility quite complicated. The most noticeable difference from LV2 and CLAP is that we cannot simply provide functions without IPC (Binder on Android), as the host cannot simply load plugin extension implementations via `dlopen()`.

## Scope of Openness

Conceptually AAP could come up with multiple implementations i.e. other implementations of `org.androidaudioplugin.AudioPluginService` than our `androidaudioplugin.aar`, but that is not what we aim right now. Therefore, unlike other plugin extensibility design, our extensions are totally based on `libandroidaudioplugin` implementation.

We could simply give up extensibility and decide that all the features are offered in AAP framework itself, like AudioUnit does not provide extensibility. But we still need some extensibility for AAP framework development for API stability.

### supported languages

Extensions API for plugin developers and Extension Service API for client host developers are so far both in C API. (C is acqually somewhat annoying and we could resort to C++ for extension service API, but let's see if we end up to make another decision.)

C is chosen for interoperability with other languages. Though other languages like Rust, Zig, or V (anything feasible for audio development). Though we haven't really examined any language interoperability yet.

## Known design problem.

We will have to revisit messaging implementation and replace it with MIDI 2.0 based messaging. For details, see https://github.com/atsushieno/android-audio-plugin-framework/issues/104#issuecomment-1100858628

At this state we haven't worked on this known issue because that would depend on fundamental [port design changes](NEW_PORTS.md).


## Extension API design: Who needs to implement what

For an extension, users are plugin developers and host developers. They want to implement simple API. They are not supposed to implement every complicated IPC bits that AAP extensions actually need under the hood. Thus, extension developers are supposed to offer not just the API definition, but also IPC implementation.

<img src="images/extensions-fundamentals.drawio.svg" />

### How plugin developers use extensions

Plugin developers need to "implement" the plugin extension functions, defined as function pointers, by themselves, and that can be fully local (without any IPC concern).

For example, consider Presets extension. Plugins that support this extension will have to provide "get preset" operation, and to supplement memory management work the extension API also needs to define "get preset size" operation. We also need preset catalog, and thus "get preset count" is needed. We also need a preset metadata entry retriever (without data) - but this can be unified to "get preset" oepration. Lastly, (I had hesitated for long time because of design ambiguity, but) we add "get current preset index" and "set current preset index" operations. Thus it looks like:

```
	typedef struct {
		int32_t index{0};
		const char *name;
		void *data;
		int32_t data_size;
	} aap_preset_t;

	typedef struct {
		void *context;
		int32_t get_preset_count(AndroidAudioPlugin* plugin);
		int32_t get_preset_size(AndroidAudioPlugin* plugin, int32_t index);
		get_preset(AndroidAudioPlugin* plugin, int32_t index, bool skipBinary, aap_preset_t* result);
		int32_t get_preset_index(AndroidAudioPlugin* plugin);
		void get_preset_index(AndroidAudioPlugin* plugin, int32_t index);
	} aap_presets_extension_t;
```


Here is an example plugin that supports "Presets" extension:

```
	struct {
		aap_preset_t **foo_presets;
		int32_t num_presets;
	} FooPluginData;

	FooPluginData pluginData;
	
	void foo_plugin_factory_initialize(FooPluginData *data) {
		// initialize foo_presets from resources etc. here.
	}

	// The actual Presets extension implementation for Foo plugin
	aap_preset_t* foo_presets_get_preset(AndroidAudioPlugin* plugin, int32_t index) {
		auto data = (FooPluginData*) plugin->plugin_specific;
		return index < data->num_presets ? foo_presets[index] : NULL;
	}

	// ... snip other presets member functions...

	// AAP Presets extension (instance) that this Foo plugin implements
	aap_presets_extension_t foo_presets_extension { NULL, /* snip other members...*/, foo_presets_get_preset };

	// `get_extension` implementation for Foo plugin
	void* foo_get_extension(AndroidAudioPlugin *plugin, const char *extensionURI) {
		// uri_to_id() implemented somewhere (outside this snippet).
		switch (uri_to_id(extensionURI)) {
		case AAP_PRESETS_EXTENSION_URI_ID:
			foo_presets_extension.context = plugin;
			return &foo_presets_extension;
		}
	}

	// Foo plugin instance used by the factory (outside this snippet).
	AndroidAudioPlugin* foo_factory_instantiate(...) {
		auto data = new FooPluginData();
		foo_plugin_factory_initialize(data);
		return new AndroidAudioPlugin(
			data,
			foo_prepare,
		 	(...),
			foo_get_extension);
	}
```

A plugin that supports some extension needs to provide `get_extension()` member of `AndroidAudioPlugin`, and it is supposed to return a corresponding extension to the argument extension URI. In this example, `foo_get_extension()` implements this and returns `foo_preset_extension` for the Presets extension URI (strictly speaking, ID int value of the URI).

The extension is then cast to the appropriate type (in the snipped above it is `aap_preset_extension_t`) and its member is to be invoked by the host (actually not directly by host, but it is explained later).


### How client (host) developers use extensions

Plugin host developers would also like to use simple API, and they are not supposed to implement the complicated IPC bits. Hosts don't have to implement the extension function members, but they cannot load the plugin library. So, hosts use IPC proxies that the extension developers provide instead.

Therefore, the host code looks like this:

```
	aap::RemotePluginInstance* pluginInstance;
	auto presetsProxy = (aap_preset_extension_t*) pluginInstance->getExtension(AAP_PRESETS_EXTENSION_URI);
	aap_preset preset;
	preset.data_size = presetsProxy->get_preset_size(0);
	presetsProxy->get_preset(0);
```

`AndroidAudioPluginHost` struct is an abstraction in AAP plugin API that works as a host feature facade. The facade offers `get_extension` function which is supposed to return an implementation of the Presets extension that delegates to "plugin service extension" instance.

It is what I described "IPC proxies" earlier, but we need a lot more explanation.

### What Extension developers need to provide

As I wrote earlier, plugin client (host) has to invoke the extension functions via proxies, and it is (should be) actually offered by the extension developers.

It should also be noted that the messaging framework part that we (AAP developers) implement is actual host code agnostic, which means that we have to make it generic and extension agnostic i.e. we cannot simply switch-case per our official extensions.

Similarly to client side, our AudioPluginService native internals also have to be implemented as extension agnostic.

Therefore we come up with some generic code like this. `onInvoked()` is invoked by AudioPluginService internals, and `getPreset()` in this example is ultimately called by extension host developers (as in `aap_preset_extension_t::get_preset` function member).

```
	void (*aap_service_extension_on_invoked_t) (
		AndroidAudioPluginHost *service,
		AAPXSServiceInstance* serviceExtension,
		AndroidAudioPluginExtension* data);

	void* (*aap_service_extension_as_proxy) (AndroidAudioPluginHost *service);

	typedef struct {
		const char *uri;
		aap_service_extension_on_invoked_t on_invoked;
		aap_service_extension_as_proxy as_proxy;
	} aap_service_extension_t;
```

Example use of this API: Presets PluginService extension implementation:

```
	// It is already in aap-core.h
	typedef struct {
		const char *uri;
		int32_t transmit_size;
		void *data;
	} AndroidAudioPluginExtension;

	// implementation details
	class PresetExtensionService {
		aap_preset_extension_t proxy;

	public:
		const int32_t OPCODE_GET_PRESET = 0;

		void onInvoked (
			AndroidAudioPluginHost *service,
			AAPXSServiceInstance* serviceExtension,
			AndroidAudioPluginExtension* data) {
			switch (opcode) {
			case OPCODE_GET_PRESET:
				int32_t index = *((int32_t*) ext->data);
				auto presetExtension = (aap_preset_extension_t*) service->get_extension(getURI());
				auto preset = (aap_preset_t*) presetExtension->get_preset(index);
				// write size, then data
				*((int32_t*) ext->data) = preset->data_size;
				memcpy(ext->data + sizeof(int32_t), preset->data, preset->data_size);
				return;
			}
			assert(0); // should not reach here
		}

		void getPreset(int32_t index) {
			*((int32_t*) extensionInstance->data) = index;
			clientInvokePluginExtension(data, OPCODE_GET_PRESET);
			int32_t size = *((int32_t*) extensionInstance->data);
			result->copyFrom(size, *((int32_t*) extensionInstance->data + 1);
		}

		void* asProxy(AndroidAudioPluginHost *service) {
			proxy.context = this;
			proxy.get_preset = std::bind(&PresetExtensionService::getPreset, this);
			return &proxy;
		}
	};

	PresetExtensionService svc;

	aap_service_extension_t presets_plugin_service_extension{
		AAP_PRESETS_EXTENSION_URI,
		std::bind(PresetExtensionService::onInvoked, svc),
		std::bind(PresetExtensionService::asProxy, svc)};
```


### What libandroidaudioplugin needs to provide

Each AAP extension service makes use of some framework part. There are two parts that libandroidaudioplugin should care.

(1) The PluginService API: each plugin client host needs to provide base implementation for extension messaging.


At this state, it is C++; we are unsure if it will be ported to C:

```
	class AAPXSServiceInstance {
		virtual const char* getURI() = 0;
		void clientInvokePluginExtension(AndroidAudioPluginExtension* ext) {
			// transmit data via Binder
		}
		virtual void onServicePluginExtensionInvoked(AndroidAudioPluginExtension* ext) = 0;
	};
```



(2) Plugin extensions need a registry so that host client can work with those plugins via their proxies.


```
	class ExtensionRegistry {
		virtual AAPXSServiceInstance* create(const char * uri, AndroidAudioPluginHost* host, AndroidAudioPluginExtension *extensionInstance) = 0;
	};
```

Extension service implementation needs to be compiled in the host. For plugins, only the extension header is required (the API has to be "implemented" by the plugin developer anyways).

A host application has to prepare its own `ExtensionRegistry` instance, and manage whatever extensions it wants to support. `StandardExtensionRegistry` should come up with all those standard extensions enabled.


## public extension API in C

We abbreviate AAP Extension Service as AAPXS. It is going to be too long if we didn't. Also, the abbreviated name would give "it's not for everyone" feeling (only extension developers and AAP framework developers use it).

They must represent everything in the public API (including ones in aap-core.h). Instead, they don't have to expose everything.

- Extension definitions
  - AAPXSFeature
- Extension instances
  - AAPXSServiceInstance
  - AAPXSClientInstance

## C++ Implementation

They are integrated to audio-plugin-host API, which is still unstable as a whole.

They have to be representable in the public AAPXS API


### List of the extension types

OBSOLETE: it needs updates!

Public C API (plugin / extensions API):

| type | header | description | status |
|-|-|-|-|
| AndroidAudioPluginExtension | aap-core.h | contains shared data ptr, size, and uri. | do we really need this? |
| AndroidAudioPluginHost | aap-core.h | facade to aap::PluginClient for plugin users | maybe outdated? |
| AndroidAudioPluginClient | extensions.h | facade to aap::PluginClient, for host developers | TODO |
| AAPXSServiceInstance | extensions.h | service extension instance. contains context, uri, and data (AndroidAudioPluginExtension). | partly outdated maybe? |
| AAPXSClient | facede to aap::PluginClient for extension developers. `extension_message()` | up to date |

Public C++ API (aap/core/host .i.e client):

| type | header | description | status |
|-|-|-|-|
| PluginExtensionServiceRegistry | extension-service.h | The collection of extension service definitions | maybe done |
| PluginServiceExtensionFeature | extension-service.h |
| PluginServiceExtension | 

Public C API vs. C++ wrapper API

| C++ | C |
|-|-|
| aap::PluginExtensionFeatureImpl | AndroidAudioPluginExtensionFeature |
| aap::PluginClientExtensionImplBase | - |
| aap::PluginServiceExtensionImplBase | - |
| aap::PluginExtension | AndroidAudioPluginExtension |
| | AndroidAudioPluginServiceExtension |


