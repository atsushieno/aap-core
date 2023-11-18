# AAPXS and Extensibility

DRAFT DRAFT DRAFT

This documentation was written when we came up with AAPXS "v2", introduced in AAP 0.8.0: it has revamped the entire extension foundation (concepts, binary transmits, and C/C++ API).

## AAP extensibility basics

In a typical extension API in a plugin format, it is a set of API (data types + functions) without implementation. In AAP, plugin extension implementation (native code) is not loadable within the host (technically it is possible on Android platform, but Google Play Store will kick out your app), and thus we have a proxying system that transmits the requests and responses.

AAP since 0.7.8 handles that via Binder IPC or MIDI 2.0 SysEx8. Each extension comes with a proxying support that the extension developer needs to implement (though most of the extensions are "standard" and AAP framework developers implement them), and those proxies are called **AAPXS** (AAP Extension Service).

### AAP extension APIs that has existed since v1

A plugin extension is implemented by each plugin. A host retrieves an extension via `get_extension()` member function in `AndroidAudioPlugin`, identifying it by the extension URI or the corresponding "URID" integer.

For quick look, these are the Standard Extensions that had existed since AAPXS v1:

- `parameters.h` : parameters support
- `midi.h` : MIDI 2.0 mappings etc.
- `state.h` : state support
- `presets.h` : presets support
- `gui.h` : GUI support

There is a few more not-implemented extensions that I would not say part of the Standard:

- `plugin-info.h` : dynamic plugin information
- `port-config.h` : dynamic port configuration

### AAPXS Registry: the extension catalogs

In both AAPXS v1 and AAPXS v2 there are "catalog" structures (classes) that the hosting implementation manages. It is used to organize the extensions the host and plugin service supports, and functions as the interface between the host (client) and the plugin (service).

### Basic AAPXS workflow

AAP extension in `AndroidAudioPlugin` needs to be returned via `get_extension()` member function. Similarly, AAP host extension needs to be returned via `get_extension()` in `AndroidAudioPluginHost`.

Plugin extensions are invoked by `AudioPluginService` (in Kotlin vocabulary), either via `extension()` AIDL method or `process()` handler implementation that would parse AAPXS SysEx8 and call the extension function maybe asynchronously.

### Problems that were addressed in AAPXS v2

Those extensions were designed when there was no consideration for realtime support. Everything was synchronous. The plugin formats I have learned to design AAP had no consideration on serious realtime support within themselves. The host is supposed to manage their own non-RT thread that has to interact with the audio thread at some stage, and the plugin extension function could be directly invoked within the host process. This means, both the host and the plugin could use the same extension API to implement (plugin) or to use/call (host).

In AAP the host and the plugin has to interact with each other, and the channel is limited. Especially, when it is processing audio in real time, `process()` with `aap_buffer_t` (audio + MIDI buffers) is the only way for interaction.

### API and ABI compatibility

AAP Extensions are defined in C API for ABI compatibility i.e. the extension implementation functions in a plugin must be dynamically loadable regardless of what compiler tools and languages the plugin developer uses.

This will make extension implementation independent of specific AAP versions to the greatest range.

### Asynchronous API

Typical plugin extension APIs are usually synchronous i.e. their functions block throughout the execution, but AAP extension foundation is designed to be "async ready", especially when the plugin enters ACTIVE = realtime mode.

It is because AAP, unlike those desktop plugin APIs, needs interaction between the host and plugin and keeping a thread per extension function call costs potentially a lot of synchronization constructs e.g. 100 locks per 100 `getPreset()` calls or potentially more (100 locks in the host + 100 locks in the plugin + 100 locks in AAPXS).

They can be designed and implemented in synchronous manner (we actually have synchronous API as some transitive solutions), but then there is no assured realtime safety.


## vNext API Design Considerations

There are some principles I have in mind. Some of them are rephrased later:

- The extensibility foundation should be extensible to third party developers outside "AAP (framework) developers".
- ABI stability is more important than usability. Usability could be achieved at wrapper libraries whose ABI does not matter and the API can be backward incompatible.
- Uses of the plugin extension API should be simple (it is easier to achieve because they can be synchronous API).
- The AAPXS API had better be simple, but it also needs to provide asynchronous API.
- AAPXS Runtime API should be kept minimum, basically they should only implement AAPXS invocation stuff.
  - It needs vast improvements from the current state.
- AAPXS Serialization API should be kept minimum too, basically they should only implement AAPXS serialization stuff.
- We need support for strongly-typed API in both host side and plugin side (but it does not have to be a uniform implementation). A typical plugin side use case is Native UI implementation (we already use them).
  - Existing implementation needs presets support, and it will need state support too. There might also be further demands for other extension functionality.

To make extensions generally usable, AAP defines some public API for AAPXS:

- The runtime API is defined in C.
- AAPXS implementation will be strongly-typed C++ API.
- Everything that needs to reside in public API is defined in `aapxs.h`
- AAPXS is designed to be host implementation agnostic: it should work with any version of libandroidaudioplugin or any other AAP-compatible host that supports AAPXS
  - existing implementation achieves AAPXS themselves, but extension providers are not.


## Users of AAPXS: host implementation and framework implementation

The client instance is used by a host to call the host-side extension API (might be different from the plugin extension API) and wait for the result, potentically asynchronously.


The service instance is used by `AudioPluginService` to dispatch an incoming request (either via a Binder IPC call or an AAPXS SysEx8) to the actual plugin implementation of the corresponding extension function, and return the results back to the outgoing channel (IPC / SysEx8).


## vNext Extension API Foundation

In vNext API, there are the following concepts (implementation can be in C++):

- Plugin Extension API (C) - close to what exists now
- Host Extension API (C) - close to what exists now
- AAPXS Runtime API (C) - used by each AAPXS implementation (`aapxs.h`)
- AAPXS Definition API (C) - each extension developer is supposed to implement and provide it
- AAPXS hosting runtime API (C++) - hosting runtime for the framework (reference implementation) (`aapxs-hosting-runtime.h`)
- Strongly-Typed AAPXS and collection of those (C++) - refresh version of `StandardExtensions` etc.

## vNext Client Side Hosting

A host implementation wants to get a generic AAPXS handler per extension by URI (but NOT per instantiated plugin - multiple instances with different instanceIds should work). In AAPXS v1 they were `AAPXSClientInstance` and `AAPXSServiceInstance`, managed by `AAPXSClientInstanceManager` etc. The responsibility boundary of those components were unclear, so they need to be reorganized.

In the new AAPXS v2 API, there are `AAPXSInitiatorInstance` (for plugin AAPXS) and `AAPXSRecipientInstance` (for host AAPXS) - they provide functions that AAPXS developers needs to access when they implement `AAPXSDefinition`.

`aap::RemotePluginInstance` provides `getAAPXSDispatcher()` that returns a derived instance of `AAPXSDispatcher`, which provides these `AAPXSInitiatorInstance` and `AAPXSRecipientInstance` explained above.

There should be two kinds of accesses to AAPXS from `RemotePluginInstance`: AAPXS runtime API and strongly-typed AAPXS API. For the Standard Extensions, there are strongly-typed API. For others, only AAPXS runtime API is the way to go. Still, if an `AAPXSDefinition` for an extension is provided, then its request handlers do the job for the extension by its own. Host developers can dynamically add the `AAPXSDefinition` to its `AAPXSDefinitionRegistry` instance. 

### vNext Hosting entrypoint to AAPXS Runtime API

Since AAP sends extension controllers either via Binder IPC (non-RT) or AAPXS SysEx8 (RT), unknown extensions can be still *supported*. For untyped extension accesses, `RemotePluginInstance` provides `sendExtensionRequest()`.

`sendExtensionRequest()` takes `AAPXSRequestContext` which contains *already serialized* requests as a binary chunk in its member `AAPXSSerializationContext`. Then it dispatches the request to the appropriate handler: Binder IPC at INACTIVE (non-RT) state, or AAPXS SysEx8 at ACTIVE RT state.

Serialization is handled by each AAPXS. For such a hosting implementation that does not directly support the extension (or a plugin implementation that does not directly suppot the host extension), there is untyped AAPXS API that is implemented without strongly-typed extension API. The host can still convey and even invoke its dynamically assigned extension invocation handler in `AAPXSDefinition`.

At AAPXS Runtime level, there are utility functions in `aap_midi2_helper.h` for AAPXS parsing and generation in C, and `AAPXSMidi2RecipientSession` and `AAPXSMidi2InitiatorSession` as the internal helpers in C++ in `aapxs-hosting-runtime.h`. To support asynchronous invocation under control, a host can assign an async callback `aapxs_completion_callback` to `AAPXSRequestContext`, which is then invoked when `AAPXSMidi2InitiatorSession` receives a corresponding reply to the request.


## vNext: what AAPXS developer writes

Each AAPXS developer provides the following stuff:

- Plugin Extension API (if applicable)
- Host Extension API (if applicable)
- AAPXS untyped runtime implementation, represented as an `AAPXSFeatureVNect` variable
  - host developers can pass it to `AAPXSFeatureRegistry::add()`
  - `process_incoming_plugin_aapxs_request()` implements plugin service request handler which deserializes AAPXS binary and invokes corresponding plugin extension feature
  - `process_incoming_plugin_aapxs_reply()` implements plugin client reply handler which serializes AAPXS binary and handles corresponding plugin extension feature
  - `process_incoming_host_aapxs_request()` implements plugin client request handler which deserializes AAPXS binary and invokes corresponding host extension feature
  - `process_incoming_host_aapxs_reply()` implements plugin service reply handler which serializes AAPXS binary and invokes corresponding host extension feature
- Strongly typed plugin extension client (optionally) to help plugin client development.
- Strongly typed host extension client (optionally) to help plugin service development.

`samples/aapxssample` is an example plugin that provides its own AAPXS (no dedicated host client app there yet though).

### vNext: Untyped Extension Handler implementation

We need AAPXS implementation by extension API developer at:

- strongly-typed plugin client AAPXS request handler: it needs to implement the strongly-typed extension API to serialize the request and send it via `send_aapxs_request()` function (in `AAPXSInitiatorInstance`) which is assigned by the host.
  - It should also provide a result parser as in `aapxs_completion_callback` and pass it to `AAPXSRequestContext`, otherwise the call results are ignored.
- weakly-typed plugin service AAPXS request handler: it needs to deserialize the AAPXS binary (either from shm on Binder IPC or SysEx8) of the incoming request, which is performed by `process_incoming_plugin_aapxs_request()` (in `AAPXSDefinition`) implemented by AAPXS developer. It takes the `requestId`, and might or might not reply back to the client.
- weakly-typed plugin service AAPXS reply sender: Once the plugin extension function does its job, then the result should be serialized by `process_incoming_plugin_aapxs_request()` or whatever it asynchronously invokes at its completion. It should then call `send_aapxs_reply()` function (in `AAPXSServiceInstance`) which is assigned by the host. It takes `requestId` for correlation.
- weakly-typed plugin client AAPXS reply handler: a caller would need to deserialize the AAPXS binary and continue the client extension call, but it is totally optional.
  - If it was an asynchronous call, then the `aapxs_completion_callback` should be invoked.

