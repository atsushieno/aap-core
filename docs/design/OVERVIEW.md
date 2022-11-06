
## Framework design

### Extensibility

AAP is extensible to some extent; plugins can query host for extension data which is represented as a pointer on shared memory (explained later), associated with certain extension URI. It is similar to how LV2 extensions works, but in AAP it is limited to **data** pointers (also without RDF and Turtle complication). Those extensions can be used only with supported hosts. A host can query each plugin whether it supports certain feature or not and disable those not-supported plugins, and a plugin can query the host which features it provides. It is limited to data i.e. no runnable code, because code in host application is simply not runnable on the plugin application.

VST3 specifics, LV2 specifics, etc. should be processed by (so-called) plugin wrappers, with some help with extensions, if provided by anyone.

### Plugin metadata format and expressiveness

We end up using XML for the metadata format (`aap_metadata.xml`).

LV2 uses Turtle format, which is [questionable](https://drobilla.net/2019/11/11/lv2-the-good-bad-and-ugly.html) in that the format brings in extraneous learning curve. Even if Turtle itself is alright, having only serd and sord as the actual parser/loader implementation is problematic.

A better alternative is obviously JSON, but for AAP there is a constraining reason to use XML - we have to retrieve plugin metadata contents, and only XML meets these two constraints:

- We don't have to start and bind Service to retrieve metadata.
- We can query metadata content (like [PackageItemInfo.loadXmlMetadata()](https://developer.android.com/reference/android/content/pm/PackageItemInfo#loadXmlMetaData(android.content.pm.PackageManager,%20java.lang.String)))

They are important in that if they are not met then hosts will have to cache plugin metadata list like VST hosts. LV2 design is clever in this aspect.

On a related note, the actual parameter names were designed like:

```
#define AAP_PORT_BASE "org.androidaudioplugin.port"
#define AAP_PORT_DEFAULT AAP_PORT_BASE ":default"
```

which were like that it can be directly used as XML namespace prefix (because BASE does not contain a ':') while keeping things pseudo-globally-unique, or JSON object keys.
Since we were using an XML parser that does not support XML Namespaces (tinyxml2), it could work like a hacky solution.
But since we cannot switch to JSON anyways, we rather went back to the basics and eliminated any XML parsing bits from native code on Android, and used authentic XML parser (libxml2) on desktop.

At that stage, there is no point of avoiding standard URI format for port property identifiers, so we simply use "urn:" for the parameters. (It can be "http:", but I guess it would be weird if the protocol without 's' will stay forever and if it matches "cool URLs don't change." concept...)

### Metadata retrieval, XML as the master metadata, not code

AAP expects plugin metadata as in declarative manner. Some other plugin formats such as CLAP, on the other hand, retrieves metadata from code. And it is implemented by the plugin developer to return precise information.

AAP so far does not expect plugin developers to return such metadata by code. It is going to lead to inconsistency in metadata (code vs. XML). It is trivial to "fix" such an issue by automated metadata code generator as in custom Gradle task, but host-based information suffices... so far, it is rather provided by the `AndroidAudioPluginHost*`, via the `plugin-info` host extension.

### out-process model

Unlike in-host-process plugin processing, switching process context is important. Considering the performance loss and limited resources on mobile devices, it is best if we can avoid that. However it is inevitable. It will be handled via [NdkBinder](https://developer.android.com/ndk/reference/group/ndk-binder).

An important note is that NdkBinder API is available only after Android 10. On earlier Android targets the binder must be implemented in Java API (or through non-public native API, which is now prohibited). At this state we are not sure if we support compatibility with old devices. They are not great for high audio processing requirement either way.

A Remote plugin client is actually implemented just as a plugin, when it is used by host i.e. it does not matter how the plugin is implemented. Though at this state we are not sure if we provide direct native client API. As of writing this, there is no way to bind services (`AudioPluginService`s) using ndkbinder API. So far we use Java binder API instead.


### Shared memory and fixed pointers

This is about the internal implementation details, not about the plugin API.

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

Although this AOSP documentation on [Binder IPC](https://source.android.com/devices/architecture/hidl/binder-ipc#rt-priority) states that real-time priority inheritance is disabled in the frameworks and it is unclear how bloadly this applies.

### MIDI input support

MIDI ports are different from audio ports and control ports in some ways. Audio and control data ports receive contiguous "raw" data which are direct output values to the destination or input values from the source, but MIDI inputs are from events with specific duration per event, or immediate direct input. On the other hand, they usually have to be processed in sync with audio inputs because (1) those MIDI inputs can be controllers (think about audio effects) and (2) audio processing should be highly efficient so that the number of processing requests must be kept as minimum as possible. Therefore usually audio plugin frameworks process audio and MIDI data at a time, and we follow that.

The approach to process MIDI data is similar between audio plugin frameworks, but the actual content format is different for each framework. Usually they come up with structured message format (VST/AU/LV2 Atom), but AAP so far expects a "length-prepended" SMF track-like inputs which are just sequence of pairs of delta time and MIDI event for "MIDI" channel. AAP does not limit the format to audio and MIDI - the port content can be anything. It is up to agreement between plugins and hosts. The MIDI data length is provided as size in bytes in 32-bit signed integer.

(A related note: to make direct inputs from MIDI devices not laggy, the audio processing buffer should be kept as minimum as possible when dealing with MIDI inputs. That in general results in high CPU (and thus power) usage, which is usually problematic to mobile devices. There should be some consideration on power usage on each application.)

The first specification of the MIDI buffer format is as follows:

- First 4 bytes: int32_t for time division specifier, which is the same as the division specifier in SMF MIDI header chunk.
- Next 4 bytes: the length of rest of MIDI buffer in bytes.
- the rest of the bytes: list of MIDI messages, where each of MIDI message is a pair of length and MIDI event.

Note: JUCE sends timestamp values as application-dependent, meaning that something must be externally spdcifyable to host and they have to be accessible from each audio plugin i.e. through some API.



