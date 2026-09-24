# AAP interaction between client and service

The basic messaging between AAP client (host) and service is achieved using NdkBinder and "AAP AIDL".

## Service connections

A client connection is made to one AudioPluginService component, identified by its package name and class name. A plugin package may have more than one AudioPluginService, each running in its own process (see "Running plugins in separate processes" in `DEVELOPERS.md`), so the package name alone does not identify a service. For a plugin, the service is `PluginInformation.packageName` and `PluginInformation.localName`:

- Kotlin: `AudioPluginClientBase.connectToPluginService(pluginInfo)` (or `(packageName, className)`), `AudioPluginServiceConnector.findExistingServiceConnection(packageName, className)`.
- native: `PluginClient::connectToPluginService()`, `PluginClientSystem::ensurePluginServiceConnected(connections, packageName, className, callback)`, `PluginClientConnectionList::getServiceHandleForConnectedPlugin(packageName, className)`.

The package-only variants remain for compatibility. As they do not tell which AudioPluginService is needed, they connect the package's *primary* service: the one declared with the stock `org.androidaudioplugin.AudioPluginService` class, or the first one if there is none (`AudioPluginHostHelper.selectPrimaryAudioPluginService()`). Plugins hosted by the other services are only reachable with the service class name. A native `ensurePluginServiceConnected()` request is always completed (its callback is invoked) once the requested service is connected, including when it was already connected; a host that then instantiates a plugin of another service gets an error ("Plugin service is not started yet"), not a hang.

On the service side, there is one AudioPluginService per process (the native service binder is a process-wide singleton). Each service lists the plugins of its own `aap_metadata.xml` (`#Plugins` for the primary service, `#SecondaryPlugins` for the others), and `beginCreate()` rejects a plugin that another service of the package hosts, so that a misrouted request never loads that plugin's library into the wrong process.

## AAP AIDL

The AIDL provides primitive operations between client and service, namely:

- `setCallback()` : sets callback (explained below)
- `beginCreate()` : begins instantiation
- `addExtension()` : adds an extension
- `endCreate()` : ends instantiation
- `prepareMemory()` : sets one port shared memory buffer
- `prepare()` : finishes preparing shared buffers
- `extension()` : performs extension operation in non-realtime mode
- `activate()` : activates the plugin instance for realtime processing
- `process()` : processes audio in realtime
- `deactivate()` : deactivates the plugin instance from realtime processing
- `destroy()` : terminates the plugin instance

## non-realtime Bi-directional messaging

Bi-directional messaging will be enabled by sending `IAudioPluginInterfaceCallback` callback interface via `setCallback` request in AAP AIDL. It is important to note that it has to be done only once per plugin service connection, not per plugin service instance.

It makes it possible to initiate an extension messaging from plugin service side, especially for non-realtime event notifications (such as parameter changes).

For realtime event notifications (if ever any), it should be performed in each `process()` operation and MIDI output port should be used instead. Typical parameter changes could be notified in non-realtime manner.

## Instancing workflow

AAP service begins with "create" operation. In AIDL it is split into three operation: `beginCreate()`, `addExtension()`, and `endCreate()`. Conceptually it could be understood like `create_instance(list<aap_extension>)` (pseudo code). Extension (at this state) usually comes up with a shared memory FD with its size.

For consistency in the future, the order of calls to `addExtension()` should be considered as significant (i.e. the extension list is ordered). Any extension that could affect other extensions should be added earlier.

Once AAP instance is created, it will start configuring plugin. The most important part is port configuration. The client and service needs to determine what kind of ports will be used, for both "audio buses" and "MIDI ports".

A traditional AAP service doesn't provide MIDI ports unless it was an instrument (synth) which is simply impossible to construct a bidirectional messaging transport.

Some extensions will be used to achieve this negotiation.

Once client and service got agreement, client starts sending `prepareMemory()` requests, to prepare port shared memory buffers. Once every port is prepared, it sends `prepare()` to finish it. These processes could be understood like `prepare(list<shard_memory_fd>)` (pseudo code).

Once all ports are ready and it is preapared, then it can be activated by `activate()` to be ready for audio processing (by `process()` method). Audio processing would be ignored if it is not at activated state (it may not; AAP cannot prevent services to process audio at inactive state).


