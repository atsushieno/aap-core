# AAP Extension Developer Guide

This document explains the AAP Extension Service API a.k.a. "AAPXS" API.

(We abbreviate AAP Extension Service as AAPXS. It is going to be too long if we didn't. Also, the abbreviated name would give "it's not for everyone" feeling, which is scary *and* appropriate. Only extension developers and AAP framework developers should use it.)

## What Extensibility means for AAP

AAP is designed to be extensible, which means, anyone can define features that can be used by both host and client. Lots of AAP features are implemented as AAP Extensions. Plugin extensbility is conceptually similar to VST-MA, LV2, and CLAP Extensions.

On the other hand, AAP is unique in that it has to work **across separate processes** (host process and plugin processes), which makes extensibility quite complicated. The most noticeable difference from LV2 and CLAP is that we cannot simply provide functions without IPC (Binder on Android), as the host cannot simply load plugin extension implementations via `dlopen()`.

Therefore, any plugin extension feature has to be explicitly initialized and declared at client host, and passed to plugin service through the instantiation phase. Host can provide a shared data pointer which can be referenced by the plugin at any time (e.g. its `process()` phase).

## Scope of Openness

Conceptually AAP could come up with multiple implementations i.e. other implementations of `org.androidaudioplugin.AudioPluginService` than our `androidaudioplugin.aar` or `libandroidaudioplugin.so`, but that is not what we aim right now. Therefore, unlike other plugin extensibility design, our extensions are totally based on `libandroidaudioplugin` implementation.

We could simply give up extensibility and decide that all the features are offered in AAP framework itself, like AudioUnit does not provide extensibility. But we (AAP framework developers) still need some extensibility for AAP framework development for API stability. Thus extensibility framework matters for us.

### supported languages

Both Extensions API (for plugin developers and host developers) and AAPXS API (for extension developers) are in C API. In the actual standard extensions implementation, we use C++ with C API wrapper.

C is chosen for interoperability with other languages. Note that only languages like C++, Rust and Zig are feasible for audio development. And we haven't really examined interoperability with those languages (yet).


## Extension API design: Who needs to implement what

For an extension, users are plugin developers and host developers. They want to implement simple API. They are not supposed to implement every complicated IPC bits that AAP extensions actually need under the hood. Thus, extension developers are supposed to offer not just the API definition, but also IPC implementation.

<img src="images/extensions-fundamentals.drawio.svg" />

### How plugin developers use extensions

Plugin developers need to "implement" the plugin extension functions, defined as function pointers, by themselves, and that can be fully local (without any IPC concern).

For example, consider "Presets" extension. Plugins that support this extension will have to provide the following features -

- "get preset" operation that transmits data content
- to supplement memory management work the extension API also needs to define "get preset size" operation
- We also need preset catalog, and thus "get preset count" is needed
  - We also need a preset metadata entry retriever (without data) - but this can be unified to "get preset" oepration.
- Lastly, (I had hesitated for long time because of design ambiguity, but) we add "get current preset index"
- and "set current preset index" operations.

Thus it looks like:

```
#define AAP_PRESETS_EXTENSION_MAX_NAME_LENGTH 256

typedef struct {
	int32_t index{0};
	char name[AAP_PRESETS_EXTENSION_MAX_NAME_LENGTH];
	void *data;
	int32_t data_size;
} aap_preset_t;

typedef struct aap_presets_context_t {
    void *context;
    AndroidAudioPlugin* plugin;
} aap_presets_context_t;

typedef int32_t (*presets_extension_get_preset_count_func_t) (aap_presets_context_t* context);
typedef int32_t (*presets_extension_get_preset_data_size_func_t) (aap_presets_context_t* context, int32_t index);
typedef void (*presets_extension_get_preset_func_t) (aap_presets_context_t* context, int32_t index, bool skipBinary, aap_preset_t *preset);
typedef int32_t (*presets_extension_get_preset_index_func_t) (aap_presets_context_t* context);
typedef void (*presets_extension_set_preset_index_func_t) (aap_presets_context_t* context, int32_t index);

typedef struct aap_presets_extension_t {
    void *context;
    presets_extension_get_preset_count_func_t get_preset_count;
    presets_extension_get_preset_data_size_func_t get_preset_data_size;
    presets_extension_get_preset_func_t get_preset;
    presets_extension_get_preset_index_func_t get_preset_index;
    presets_extension_set_preset_index_func_t set_preset_index;
} aap_presets_extension_t;
```

To make it implementable in C, the functions are defined as function pointers in the structure.

Here is an example plugin that supports this Presets extension:

```
uint8_t preset_data[][3] {{10}, {20}, {30}};

aap_preset_t presets[3] {
    {0, "preset1", preset_data[0], sizeof(preset_data[0])},
    {1, "preset2", preset_data[1], sizeof(preset_data[1])},
    {2, "preset3", preset_data[2], sizeof(preset_data[2])}
};

int32_t sample_plugin_get_preset_count(aap_presets_context_t* /*context*/) {
    return sizeof(presets) / sizeof(aap_preset_t);
}

int32_t sample_plugin_get_preset_data_size(aap_presets_context_t* /*context*/, int32_t index) {
    return presets[index].data_size; // just for testing, no actual content.
}

void sample_plugin_get_preset(aap_presets_context_t* /*context*/, int32_t index, bool skipContent, aap_preset_t* preset) {
    preset->index = index;
    strncpy(preset->name, presets[index].name, AAP_PRESETS_EXTENSION_MAX_NAME_LENGTH);
    preset->data_size = presets[index].data_size;
    if (!skipContent && preset->data_size > 0)
        memcpy(preset->data, presets[index].data, preset->data_size);
}

int32_t sample_plugin_get_preset_index(aap_presets_context_t* context) {
    return (int32_t) (int64_t) context->context;
}

void sample_plugin_set_preset_index(aap_presets_context_t* context, int32_t index) {
    context->context = (void*) index;
}

// AAP Presets extension (instance) that this Foo plugin implements
void* sample_plugin_get_extension(AndroidAudioPlugin* plugin, const char *uri) {
    if (strcmp(uri, AAP_PRESETS_EXTENSION_URI) == 0)
        return &presets_extension;
    return nullptr;
}

// Foo plugin instance used by the factory (outside this snippet).
AndroidAudioPlugin *sample_plugin_new(...) {
    return new AndroidAudioPlugin{
            handle,
			...
            sample_plugin_get_extension
    };
}
```

A plugin that supports some extension needs to provide `get_extension()` member of `AndroidAudioPlugin`, and it is supposed to return a corresponding extension to the argument extension URI. In this example, `foo_get_extension()` implements this and returns `foo_preset_extension` for the Presets extension URI (strictly speaking, ID int value of the URI).

The extension is then cast to the appropriate type (in the snipped above it is `aap_preset_extension_t`) and its member is to be invoked by the host (actually not directly by host, but it is explained later).

### How client (host) developers use extensions

Plugin host developers would also like to use simple API, and they are not supposed to implement the complicated IPC bits. Hosts don't have to implement the extension function members, but they cannot load the plugin's shared library into the host application process. So, hosts use "IPC proxies" that the extension developers provide instead.

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
	class AAPXSRegistry {
		virtual AAPXSServiceInstance* create(const char * uri, AndroidAudioPluginHost* host, AndroidAudioPluginExtension *extensionInstance) = 0;
	};
```

Extension service implementation needs to be compiled in the host. For plugins, only the extension header is required (the API has to be "implemented" by the plugin developer anyways).

A host application has to prepare its own `AAPXSRegistry` instance, and manage whatever extensions it wants to support. `StandardAAPXSRegistry` should come up with all those standard extensions enabled.


## public extension API in C

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
| AAPXSRegistry | extension-service.h | The collection of extension service definitions | maybe done |
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



## Known design issues.

At this moment, the scope of extension services is limited to non-realtime request/reply messaging. If your plugin needs realtime plugin controls, then it should provide a MIDI channel and transmit any operation as in System Exclusive messages etc. there.

We may have to revisit messaging implementation and replace it with MIDI 2.0 based messaging. For details, see https://github.com/atsushieno/android-audio-plugin-framework/issues/104#issuecomment-1100858628

Hopefully can be upgraded only about internals without changing the public AAPXS API surface.

At this state we haven't worked on this known issue because that would depend on fundamental [port design changes](NEW_PORTS.md).
