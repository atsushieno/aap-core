# AAPXS and Extensibility

DRAFT DRAFT DRAFT

In a typical extension in a plugin format, it is a set of API (data types + functions) without implementation. In AAP, plugin extension implementation (native code) is not loadable within the host (technically it is possible on Android platform, but Google Play Store will kick out your app), and thus we have a proxying system that transmits the requests and responses via Binder IPC or MIDI 2.0 SysEx8. Each extension comes with a proxy implementation provided by the extension developer (though most of the extensions are "standard" and AAP framework developers implement them), and those proxies are called AAPXS (AAP Extension Service).

## Extension APIs

AAP Extensions are defined in C API for ABI compatibility i.e. the extension implementation functions in a plugin must be dynamically loadable regardless of what compiler tools and languages the plugin developer uses.

Funcsions in AAP extensions are usually synchronous: plugin extensions are invoked by `AudioPluginService`, either via `extension()` AIDL method or `process()` handler implementation that would parse AAPXS SysEx8 and call the extension function maybe asynchronously.

As of writing this documentation, these are the Standard Extensions:

- `parameters.h` : parameters
- `midi.h` : MIDI 2.0 mappings etc.
- `state.h` : state
- `presets.h` : presets
- `gui.h` : GUI support

There is a few more not-implemented extensions that I would not say part of the Standard:

- `plugin-info.h` : dynamic plugin information
- `port-config.h` : dynamic port configuration

## Problems

Those extensions were designed when there was no consideration for realtime support. Everything was synchronous. The plugin formats I have learned to design AAP had no consideration on serious realtime support within themselves. The host is supposed to manage their own non-RT thread that has to interact with the audio thread at some stage, and the plugin extension function could be directly invoked within the host process. This means, both the host and the plugin could use the same extension API to implement (plugin) or to use/call (host).

In AAP the host and the plugin has to interact with each other, and the channel is limited. Especially, when it is processing audio in real time, `process()` with audio + MIDI buffers is the only way for interaction.

## vNext API Design Considerations

There are some principles I have in mind. Some of them are rephrased later:

- The extensibility foundation should be extensible to third party developers outside "AAP (framework) developers".
- The plugin extension API should be simple (it is easier to achieve because they can be synchronous API).
- The host extension API had better be simple, but it also needs to provide asynchronous API.
- AAPXS API should be kept minimum, basically they should only implement serialization stuff.
  - It needs vast improvements from the current state.
- Whether a host or a plugin supports an extension can be statically determined i.e. we do not plan to support dynamic extensibility (loading dynamic library to support arbitrary third party extensions; they should be determined at compile time).

To make extensions generally usable, AAP defines some public API for AAPXS:

- The runtime API is defined in C.
- AAPXS implementation will be strongly-typed C++ API (since we do not have to **dynamically** load AAPXS implementation and thus no need for ABI compatibility).
- Everything is defined in `aapxs.h`
- AAPXS is designed to be host implementation agnostic: it should work with any version of libandroidaudioplugin or any other AAP-compatible host that supports AAPXS
  - existing implementation achieves AAPXS themselves, but extension providers are not.

## Existing AAPXS implementation basics

An AAPXS developer provides a pair of AAPXS implementation constructs:

- the AAPXS client instance (current API is `AAPXSClientInstance`): serialize the extension function arguments to binary buffer, which is then sent either via a Binder IPC or AAPXS SysEx8, and once the result is back then deserialize the reply. It only performs those serialization works.
- the AAPXS service instance (current API is `AAPXSServiceInstance`): deserialize the binary buffer which is either via a Binder IPC or AAPXS SysEx8, and serialize the results (primarily the return value) to the buffer.

The existing implementation of above also involves the actual extension invocation.

`AAPXSFeature` works as a facade to the extension functions. `AAPXSClientInstance` and `AAPXSServiceInstance` are instantiated by AAPXS implementation (`AAPXSClientInstanceManager::setupAAPXSInstance()`). `AAPXSClientInstanceManager` is provided by `RemotePluginInstanceStandardExtensions` which is derived from `StandardExtensions` and has a host implementation `RemotePluginInstanceStandardExtensionsImpl`.

## Users of AAPXS: host implementation and framework implementation

The client instance is used by a host to call the host-side extension API (might be different from the plugin extension API) and wait for the result, potentically asynchronously.


The service instance is used by `AudioPluginService` to dispatch an incoming request (either via a Binder IPC call or an AAPXS SysEx8) to the actual plugin implementation of the corresponding extension function, and return the results back to the outgoing channel (IPC / SysEx8).


## Extension API Foundation vNext

In vNext API, there will be these concepts:

- Plugin Extension API (C) - close to what exists now
- Host Extension API (C) - close to what exists now
- AAPXS Runtime Interface (C) - refresh version of `AAPXSClientInstance` etc., explained below
- AAPXS Runtime implementation (C++) - refresh version of `AAPXSManager` etc., explained below
- Strongly-Typed AAPXS implementation (C++) - refresh version of `StandardExtensions` etc., explained below

## vNext AAPXS Serialization API

AAPXS Serialization Interface provides foundation for message serialization. They are used by strongly typed extension API, which are not part of ABI.

In the new API, we will split those tasks into distinct parts:

- client: `AAPXSClientSerializer`
  - serializing the plugin API request and host API reply
  - deserializing the plugin API reply and host API request - it will need some type conversion via an opaque pointer, or generic return type definition in C++
- service: `AAPXSServiceSerializer`
  - deserializing the plugin API request and host API reply
  - serializing the plugin API reply and host API request

```C
typedef struct AAPXSClientSerializer {
    void (*serialize_plugin_request) (AAPXSClientSerializer* serializer, AAPXSSerializationContext* context);
    void (*deserialize_plugin_reply) (AAPXSClientSerializer* serializer, AAPXSSerializationContext* context);
    void (*deserialize_host_request) (AAPXSClientSerializer* serializer, AAPXSSerializationContext* context);
    void (*serialize_host_reply) (AAPXSClientSerializer* serializer, AAPXSSerializationContext* context);
}
```

The API should be kept as mininum as possible; every other tasks should be handled by `AudioPluginService` or a host (reference implementation anyways).

Then, there are handful of extension provider types (partly corresponds to `AAPXSFeature` but should be more like a provider API), like:

```C

typedef struct AAPXSProvider {
    void* host_context;
    void (*client_serializer_init) (AAPXSClientSerializer* client);
    void (*service_serializer_init) (AAPXSServiceSerializer* service);
} AAPXSProvider;
```

### vNext AAPXS Runtime API: handler types

The new `AAPXSProvider` above does not contain members like `AAPXSFeature::as_proxy()` or `AAPXSFeature::on_invoked()`. Should we add them too?

```C
typedef struct AAPXSSerializationContext {
    AAPXSClientHandler* handler;
    void* data;
    size_t data_size;
    size_t data_capacity;
} AAPXSSerializationContext;

typedef struct AAPXSProxyContext {
    const char* uri;
    void* extension;
    AAPXSSerializationContext serialization;
} AAPXSProxyContext;

typedef struct AAPXSClientHandler {
    AAPXSProxyContext (*as_plugin_proxy) (AAPXSClientHandler* handler, AAPXSClientSerializer* serializer);
    void (*on_host_invoked) (AAPXSClientHandler* handler, AAPXSClientSerializer* serializer, AndroidAudioPluginHost* host, int32_t opcode);
} AAPXSClientHandler;

typedef struct AAPXSServiceHandler {
    AAPXSProxyContext (*as_host_proxy) (AAPXSServiceHandler* handler, AAPXSServiceSerializer* serializer);
    void (*on_plugin_invoked) (AAPXSServiceHandler* handler, AAPXSServiceSerializer* serializer, AndroidAudioPlugin* plugin, int32_t opcode);
}

typedef struct AAPXSInstanceManager {
    void* host_context;
    AAPXSClientSerializer* (*get_client_serializer) ();
    AAPXSServiceSerializer* (*get_service_serializer) ();
    AAPXSClientHandler* (*get_client_handler) ();
    AAPXSServiceHandler* (*get_service_handler) ();
} AAPXSInstanceManager;
```

## vNext Client Side Hosting

A host implementation wants to get a generic AAPXS handler per extension URI per instantiated plugin. Previously they were `AAPXSClientInstance` and `AAPXSServiceInstance`, managed by `AAPXSClientInstanceManager` etc. The responsibility boundary of those components were unclear, so they need to be reorganized.

In the new reference host implementation, `aap::RemotePluginInstance` provides `getAAPXSManager()` that returns the `AAPXSInstanceManager`, which provides `get_client_handler()` that returns `AAPXSClientHandler`.

`AAPXSClientHandler` provides `as_proxy()` which provides opaque pointer to the strongly-typed AAPXS implementation. Host is supposed to cast the `extension` field of `AAPXSProxyContext` type to the actual AAPXS type.

There should be two kinds of accesses to AAPXS from `RemotePluginInstance`: AAPXS runtime API and strongly-typed AAPXS API. Existing extensions (such as Standard Extensions) there can be strongly-typed API. For others, only AAPXS runtime API is the way to go.

### vNext Hosting: AAPXS Runtime API

Since AAP sends extension controllers either via Binder IPC (non-RT) or AAPXS SysEx8 (RT), unknown extensions can be still *supported*. For untyped extension accesses, `RemotePluginInstance` provides `sendExtensionRequest()` (`sendExtensionMessage()` in the existing implementation).

`sendExtensionRequest()` takes `AAPXSProxyContext` which contains *already serialized* requests as a binary chunk, and dispatches it to the appropriate handler: Binder IPC at INACTIVE (non-RT) state, or AAPXS SysEx8 at ACTIVE RT state. (`processExtensionReply()` is also defined but not sure if it will be used in the public API surface after the changes.)

Serialization is in general a task for strongly-typed API for applications, so for AAPXS Runtime they are irrelevant. At AAPXS Runtime level, there are utility functions in `aap_midi2_helper.h` for AAPXS parsing and generation in C, and `AAPXSMidi2Processor` and `AAPXSMidi2ClientSession` as the internal helpers in C++.

The function sends request in asynchronous way. It optionally takes `aapxs_completion_callback` so that user of `RemotePluginInstance` can receive the completion notification.

The AAPXS Runtime will handle asynchronous calls internally regardless of the optional argument. If the function is not synchronous, then it will involve `std::promise` (if the programming paradigm matches) in the implementation (C++ will not appear in the public API surface).



### vNext Hosting: Strongly Typed AAPXS API

For strongly typed API such as `getPresetCount()` etc. :

- For any non-RT extension function, the return value has to be handled via callback. For example, `getPreset()` takes an `aapxs_completion_callback` argument that receives completion notification (the actual results are stored in its argument `aap_preset_t` so it is not "returned")
- For any RT extension function, the function can be defined to immediately return some value, but the AAPXS runtime will block the current thread until the correponding result arrives.

Instead of defining every extension functions within `RemotePluginInstance`, `RemotePluginInstance` will provide `getAAPXS<T>()`, which should return `T*` where `T` is each extension type for AAPXS.

### vNext Extension Manager (C++)

For every extension for every instance, we manage some storage e.g. for shared memory. They are managed as `AAPXSSerializationContext`.

To implement strongly-typed AAPXS at host, it queries an `AAPXSClientHandler` from `AAPXSInstanceManager` (retrieved from `getAAPXSManager()`).


## Dynamic Loading

To make AAPXS dynamically loadable, we need C API for ABI compatibility. It had been achived through `aapxs.h`, and if we stick to ABI compatibility something similar needs to remain.

A plugin could dynamically dispatch AAPXS requests (either via Binder IPC or AAPXS SysEx8: both are non-static solutions). `AudioPluginService` implementation could implement the AAPXS handlers (that unifies requests from either style and dispatches to the uniform extension entrypoints) in extension-agnostic manner.


