# designing buses, ports, and parameters

It is a documentation to describe future plans on how to change ports. NOT a documentation that describes CURRENT implementation.

## Current design: ports are ports, no further difference

Currently AAP design is kind of lost on how to deal with audio and MIDI ports, on various issues. We need to sort everything out.

AAP initially followed LV2, which had no distinct concepts on those "ports" as an entity (`void*` buffer) - a port can be of audio of primary in/out, or auxiliary port. They are distinguished by port properties and for plugins and hosts they are mere memory buffers.

## Problems

### Fixed audio ports without audio buses

Currently AAP ports all have to be pre-defined right now, which was traditionally fine when there are only stereo channels for any professional audio land (except that there are plugins only for mono channel, which require multiple instancing for no reason. It is also impossible for simple hosts/DAWs to support them). Now there are spatial audio and many speakers configurations which make this simple premise broken.

Audio ports can be actually primary or auxiliary (for sidechaining, CV ports). It is an important concept to note, otherwise we would completely kill those features if we completely removed port customization.

### Potential huge number of parameter control ports

In LV2 design, a "parameter" (conceptually) typically has a corresponding port (code entity). But if there are hundreds of parameters and thus hundreds of ports, it leads to performance problem.

And it is not only in LV2 world; VST3 MIDI CC mappings had similar issue - not by having buffered ports but hundreds of extraneous parameter retrieval in every processing function harms performance as well.

To avoid this problem, it makes more sense to have some dedicated control port that sends changed parameter values in the audio processing loop. In LV2 world it is realized as lv2:patch.

### Bi-directional MIDI ports, for multiple types and roles

Similarly, MIDI ecosystem has been changing. Modern MIDI requirements became bi-directional (MIDI-CI, especially when we support MIDI 2.0), and it is actually useful for audio plugin and hosts to have such a communication channel. One port should remain uni-directional, but they can be paired.

Although it should be noted that one single pair of MIDI input/output would not work well, because audio processing must be in realtime while there should be other MIDI messaging i.e. event inputs from controller/UI, and property exchanges. They should have completely different "channels" which are even "hidden" (as implementation details) for plugin developers, like how plugin developers deal with "states" distinctly now.

A realtime channel cannot really deal with MIDI 2.0 Property Exchange since it involves JSON whose processing (e.g. string comparison) is not feasible in realtime use. We need something more like LV2 Atom. It should also be noted that realtime channels should be activated in prior, and Property Exchange should be availabel regardless of whether it is active or not.

Plugin developers should only care about parameter definitions, property definitions, and event controller bindings to the target parameters.

### Property exchange

One missing concept in AAP is that there is no way to exchange plugin properties beyond what `aap_metadata.xml` statically describes. It is one of the reasons why I want to newly introduce MIDI-CI-like channels.

It should be although noted that some properties might be better retrieved statically (like, "is it an instrument or an effect?") to avoid extraneous plugin instantiation. For MIDI device services we could filter out effects simply by MIDI `device_info.xml` existence, but we have no corresponding concept for effects so far. It is still importatnt to keep providing `aap_metadata.xml` which offers basic information there.

### Lack of liveness checks (active sensing)

Currently there is no channel to send and receive active sensing. Desktop plugins would not need that, but AAPs are remote, therefore liveness checks would need connections.

One kind of major issue without liveness check was that, when a host crashed, the connected plugins were still alive and kept alive and consuming power forever.

### No dynamic ports

It is not really a practical issue, but there is no support for dynamic port configuration. LV2 supports Dynamic Manifest which is not really recommended but is supported. It is useful for certain kinds of plugins that loads various user defined instruments or effects (samplers, FM synthesizer emulators, user scripting) that can also configure ports. They might not be necessarily parameters on non-LV2 frameworks though.

## New design

### AAP-Next functions: new entrypoint providers

Since there will be new set of plugin functions, there will be another plugin factory function that is to provide those functions. New plugin functions are:

- `get_property(name)`
- `set_parameter(index, value)`

We'd call them **AAP-Next functions** in this document.

### AAP system messaging ports

To implement new features, we would first introduce the new communication protocol. We need two unidirectional port connections as a pair of MIDI in/out channels. **They will be established for *any* plugin**, regardless of whether the plugin is old or new. AAP will reuse `prepareMemory()` binder message to establish those MIDI-CI connection ports, with *negative* integer port number (`-1` for host-to-plugin, and `-2` for plugin-to-host). **These ports are defined to be NON-realtime safe** and **always available without active state**.

These ports are used for the following purposes:

- Active sensing (system common messages)
- Profile Configuration and Property Exchange (system exclusives)

Neither of host and plugin has direct access to those ports. Active sensing can be achieved by a host API `isPluginAlive()`. Profile Configuration and Property Exchange would need some dedicated AAP extension API (**TODO**: we will have to ensure extension APIs work, currently it has certain premise that pointers are shared among two sides and function pointers would not work).

The Plugin would be able to implement their own "property provider" `get_property(name)` AAP-Next function as an extension function, but it does not have to. AudioPluginService will implement the core part that is to receive and send those messages for MIDI-CI management channel support *itself*. But at the same time, it is very possible that other extensions (such as AudioBus Profile Configuration extension) require more entrypoint functions in the plugin.

### Audio Bus configuration

There will be "audio bus" configuration. Both host and plugin deal with this concept. To keep backward compatibility, it  is represented and implemented within the form of an AAP extension.

After this change, there would be no "audio ports" in `aap_metadata.xml` by default, which means that the audio ports are configured in the default manner. For backward compatibility, if no audio `<port>` element exists then that means "any number of audio ports can be established". The plugin is supposed to process them in their own way. Most effects would process every audio channel equally, depending on the input. Effects like traditional rotary encoders would expect stereo inputs and process them in stereo manner.

host: "instantiate plugin A"
plugin: "instantiated, ID=x"
host: "(optionally) get configurable audio buses" (in metadata)
plugin: (Androd system gives metadata that contains the list)
host: "configure audio bus X"
plugin: "success"
host: "connect ports with these shared pointers" ...

Audio bus configuration will be done by "bus index" at simplest. And it can be even skipped if it is "stereo" (default). Audio buses that can be specified by index is given by "Audio bus definition".

For now, there is no plan to work on audio bus definition right now, but it will be given by JSON, transmitted via Profile Configuration message via AAP system message channel (it is in fact sysex though). Here is the format:

```
{
  "options": {},
  "buses": ["stereo", "mono", "quad",
    {"name": "5.1", "channels": [ "L", "R", "C", "LFE", "SL", "SR" ]},
    {"name": "7.1", "channels": [ "L", "R", "C", "LFE", "SL", "SR", {"name": "CL"}, {"name": "CR"} ] }
  ]
}
```

There is no defined `options` so far. Each audio bus entry can be either by predefined name string ("stereo", "mono", "quad" here) or an object that contains:

- "name" : the bus identifier
- "discrete": true if the channels are discrete and do not expect automatic mixing
- "channels" : array of channels, where each entry can be also either a predefined name string ("L", "R", "C" ...) or an object that contains
  - "name": the bus identifier

The order of those items matter.

The predefined audio channel identifiers are specified as identical to [Web Audio 1.0 Section 4.2 "Channel Ordering"](https://webaudio.github.io/web-audio-api/#ChannelOrdering)

Hosts might not implement automatic mixing for non-discrete channels.

### Generic event ports between host and plugin

There are couple of key components that makes "parameter changes" possible:

- In the next AAP, there will be two ways to set parameters: via parameter control ports and call to `set_parameter(index, value)` AAP-Next plugin function.
- There will be **realtime event ports** that are used to send a sequence of queued events such as "set parameter" (similar to LV2 Atom Sequence of lv2:patch).
  - They do not have to be declared in `aap_metadata.xml` ; they are automatically instantiated with a fixed **negative port index** for each (`-3` and `-4`), and not directly accessible by neither of plugin nor host.
  - On every Binder `process()` invocation, AudioPluginService is supposed to parse the event inputs and call the plugin's `set_parameter()`  AAP-Next plugin function, then call the actual plugin's `process()`.
- There will be an additional host API function `setParameter(index, value)` that enqueues the message to the port.
  - It will be called by UI event receiver in AAP hosting API.

Another idea I had was that it would be possible to provide GUI editors for audio plugin parameters apart from the plugin app itself e.g. an editor in app A that controls plugin parameter of app B whose instance is hosted at app C. While it could be ideal for common patterns of GUI parameter editors (that is what MIDI 2.0 Profile Configuration kind of expects), but it requires access permission controls over the hosted plugins - otherwise plugins can be arbitrarily accessed by random other apps which potentially harms host's internal state management. It is more straightforward if the host app C receives requrests those GUI parameter editor A and forwards them to plugin app B.

### Parameters and CV ports

As mentioned a couple of time earlier, there will be a new plugin property called parameters. A `port` used to work like a parameter with its own control port. Just like port, parameters come with an index that has to be **unique** among the entire parameter indices and port indices. It is because parameters may populate ports. Also they have to be stable across plugin versions so that state can be restored with no problem.

`aap_metadata.xml` will have `<parameters>` and `<parameter>` elements just like `<ports>` and `<port>`. 

While we still allow defining `<port>` directly in `aap_metadata.xml`, parameters that are supposed to work like CV port can be indicated with `create-port='true'`  XML attribute value on the `<parameter>` element. AAP host and AudioPluginService will automatically populate a control port for each of such ports.

For host implementation, it will have to first iterate over `<parameter>`s alongside `<port>`s.

There will be `ParameterInfo` in C++ hosting API and Kotlin hosting API.

### Dynamic ports

At this state it is still not clearly supported. We might still introduce support for dynamic ports in the future. To make it happen, the usage scenario should be still limited like (1) plugin configuration and state restoration is done before port configuration, and/or (2) there should be port configuration "reset" when plugin configuration change happens, meaning that dynamic port connections are all killed off by host. This is still doable within the new design ideas above.

### Appendix: List of implicit ports

Those negative port indices (mentioned through the document) are reserved:

- `-1`: host-to-plugin system common message port
- `-2`: plugin-to-host system common message port
- `-3`: host-to-plugin realtime event port
- `-4`: plugin-to-host realtime event port (currently no plan to use though)

## Implementation steps

(1) Implement system message channels and active sensing

Setting up the channels is easier than other tasks, and some other tasks depend on it. Active sensing is optional but should not be too hard.

(2) Implement default audio port probing. Remove audio <port>s from aapinstrumentsample and get it working.

(3) Implement parameter ports. AAP-Next function loader, metadata parser. Implement event channels that are automatically created. Implement event list parser and generator.

## diffs from former issues

- https://github.com/atsushieno/android-audio-plugin-framework/issues/95
  - Nothing particular
- https://github.com/atsushieno/android-audio-plugin-framework/issues/73
  - Nothing particular. The new system message channels are non-realtime.
- https://github.com/atsushieno/android-audio-plugin-framework/issues/69
  - Nothing particular. We still keep non-audio general ports.
- https://github.com/atsushieno/android-audio-plugin-framework/issues/44
  - Still nothing to be planned, but there are some implementation ideas now.
  - From plugin to return dynamic ports information, there are now system message ports that will transmit Profile Configuration or Property Exchange.
  - some concrete C API ideas are dropped from this document, as it was written without the concept of system message channels.
- https://github.com/atsushieno/android-audio-plugin-framework/issues/80
  - It is quite close to the point where I decided to write this, yet we covered realtime requirements on this document.
  - No particular mention on `<parameter>` definitions in aap_metadata in this document yet.



