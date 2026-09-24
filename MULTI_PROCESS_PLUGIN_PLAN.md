# Multi-process plugin packages (several plugin processes in one APK)

## Status

Implemented in the working trees of aap-core, aap-juce and aap-juce-adlplug-ae (not committed
yet), and verified on a device with `aapval`, the in-app plugin manager (native UI of both
plugins), a MIDI 2.0 keyboard app, and uapmd-cmp (unmodified, i.e. using the package-only API).
Differences from the plan below:

- **One `aap_metadata.xml` per AudioPluginService, instead of one shared file with a `process`
  attribute on each `<plugin>`.** The primary service declares its file as `#Plugins`, and the
  other services as the new `#SecondaryPlugins`, which hosts built with aap-core 0.11.1 or earlier
  do not know. Found with greenhouse (built with an older aap-core): it ignores `process`, so with
  the shared file every service listed every plugin, and its plugin list crashed on the duplicate
  plugin IDs (Compose `LazyColumn` keys). Plain per-service `#Plugins` files would not be enough:
  an old host would then see OPNplug-AE under its own service, reach it with its package-only
  connection (which binds the first service), and block (see below). With `#SecondaryPlugins`,
  old hosts see only the primary service and its plugins.
- **Package-only requests connect the package's primary service**: the one declared with the
  stock `org.androidaudioplugin.AudioPluginService` class, or the first one if there is none
  (`AudioPluginHostHelper.selectPrimaryAudioPluginService()`). Plugins of the other services need
  the class-aware APIs; each instantiated plugin then binds exactly its own service. (Binding
  every service of the package was tried and rejected: with a JUCE package of 10+ plugins, one
  package-only request would start 10+ processes, each loading its JUCE library.)
- **`ensureBinderConnected()` always completes the native request**, even if the service was
  already connected. Found with uapmd-cmp (remidy): after ADLplug-AE was instantiated,
  OPNplug-AE's package-only request hit the already-connected first service, and the host waited
  forever. Now such a request fails with "Plugin service is not started yet" instead (verified
  with unmodified uapmd-cmp, in both instantiation orders).
- The JS automation runtime (`aap.instancing.create`) passes the plugin ID to the host's new
  `AapAutomationRuntime.pluginServiceConnector` hook, so that it binds the plugin's own service;
  `aap.instancing.connect(packageName)` binds the primary service.
- The new `PluginClientSystem::ensurePluginServiceConnected(connections, packageName, className,
  callback)` is declared after the existing virtual functions, and the package-only one keeps its
  place, so that the vtable layout of `PluginClientSystem` is unchanged.
- The MIDI port parser lives in aap-core as `AudioPluginMidiDeviceMetadata` (public), so that
  both the MIDI device service and `aapval` use it.
- The validator checks are `AAPVAL-PROC-001` (one service per process), `-002` (each plugin
  listed by exactly one service), `-003` (view service declared, exported, same process), `-004`
  (MIDI `plugin-id` refers to a hosted plugin) and `-005` (secondary services use
  `#SecondaryPlugins`).
- `androidx.lifecycle:lifecycle-service` and `androidx.savedstate:savedstate` became `api`
  dependencies of `androidaudioplugin`: they are supertypes of `AudioPluginViewService`, which
  plugin apps now derive from.
- Not done yet: the "Other repositories" follow-ups, and `docs/schemas` (there is no schema for
  the core `aap_metadata.xml` vocabulary to extend).

The driving use case is aap-juce-adlplug-ae, which currently ships two APKs (ADLplug-AE and
OPNplug-AE) and wants to ship one. The aap-core part of this plan is not JUCE-specific: it lets any
plugin package run its plugins in separate processes.

Paths are relative to this repository. Other repositories are referred to by name (they are
siblings in the AAP source tree).

## Background

### Why the two plugins cannot simply share one APK today

aap-juce-adlplug-ae builds both plugins from one CMake project, and both current APKs already
contain both `libADLplug-AE_Standalone.so` and `libOPNplug-AE_Standalone.so` (AGP packages every
shared-library target). Packaging is not the problem. The problem is that two JUCE-based libraries
cannot run in the same process:

- JUCE's `JNI_OnLoad()` only runs for a library loaded through `System.loadLibrary()`. AAP loads
  plugin libraries with `dlopen()`, so a second JUCE runtime never gets its `JavaVM` or app context.
- JUCE binds the native methods of shared Java classes with `RegisterNatives()`:
  `com.rmsl.juce.Java.initialiseJUCE`, and `com.rmsl.juce.ComponentPeerView.handlePaint`,
  `handleMouseDown` and so on. A process has one copy of each Java class, and the last library to
  register wins. The other plugin's UI callbacks then run against the wrong JUCE runtime, which
  has a different `Desktop` and `MessageManager`.
- aap-juce's statically exported JNI functions
  (`JuceAudioPluginViewFactory.getPreferredSize`,
  `JuceAudioProcessorEditorView.addAndroidComponentPeerViewTo`) exist in both libraries, and ART
  binds only one of them.

### Alternatives that were rejected

- **One `.so` containing both chips.** ADLplug defines the same type names for both chips
  (`Parameter_Block`, `Main_Component`, `Instrument`, `Player`, ...) and compiles its shared
  sources once per chip (`ADLPLUG_OPL3` / `ADLPLUG_OPN2`). Everything would have to be compiled
  twice in separate namespaces, which is a large patch on upstream ADLplug. aap-juce's
  `createPluginFilter()` would also need to know which plugin ID it is creating.
- **Patching JUCE to use a different Java package per library.** This is a large JUCE fork to
  maintain.

### Why separate processes solve it

Each Android process has its own ART runtime and its own copy of every Java class. If each plugin
process loads exactly one JUCE library, nothing collides.

What prevents this today is that aap-core assumes one `AudioPluginService` per package in many
places. The native connection list is already keyed by package and service class, but discovery,
binding, UI routing, MIDI device services and local lookups are keyed by package name only.

## Target layout

For aap-juce-adlplug-ae:

| Process | Components | Plugin library loaded |
|---|---|---|
| main | `PluginManagerActivity`, MIDI device services, `JuceActivity` (declared, never launched) | none |
| `:adlplug` | stock `org.androidaudioplugin.AudioPluginService` and `AudioPluginViewService` | `libADLplug-AE_Standalone.so` |
| `:opnplug` | `OpnplugAudioPluginService` and `OpnplugAudioPluginViewService` (empty subclasses) | `libOPNplug-AE_Standalone.so` |

### Rules

1. **One `AudioPluginService` per process.** The native service binder (`sp_binder` in
   `androidaudioplugin/src/main/cpp/android/AudioPluginNatives_jni.cpp`) is a process-wide
   global.
2. **Each extra process needs its own class names.** A manifest can declare a component class only
   once, and `android:process` belongs to that declaration, so one class can only run in one
   process. Extra processes therefore use empty subclasses of `AudioPluginService` and
   `AudioPluginViewService`. The *primary* process keeps the stock class names, so hosts built
   with an older aap-core can still reach it (see [Compatibility](#compatibility)).
3. **A plugin's view service runs in the same process as its `AudioPluginService`.** The native
   view is created against the in-process plugin instance, found through
   `AudioPluginServiceHelper.getServiceInstance()`.
4. **The main process never loads a plugin library.** This already holds:
   - The in-app manager and the MIDI device service instantiate plugins over binder
     (`createInstance(id, true)` in `AudioPluginNatives_jni.cpp` and
     `androidaudioplugin-midi-device-service/src/main/cpp/AAPMidiProcessor.cpp`).
   - Plugin UI is embedded through `SurfaceControlViewHost`.

   Keep it that way: do not add in-process shortcuts for plugins that happen to share the
   package.

### Manifest (aap-juce-adlplug-ae, abridged)

```xml
<application android:label="@string/app_name" android:icon="@drawable/ic_launcher">
  <activity android:name="org.androidaudioplugin.ui.compose.app.PluginManagerActivity" ...>
    <!-- MAIN/LAUNCHER -->
  </activity>
  <activity android:name="com.rmsl.juce.JuceActivity" ... />

  <!-- primary process: stock class names, declared first -->
  <service android:name="org.androidaudioplugin.AudioPluginService"
      android:process=":adlplug" android:label="AAP ADLplug-AE"
      android:foregroundServiceType="mediaPlayback" android:exported="true">
    <intent-filter><action android:name="org.androidaudioplugin.AudioPluginService.V4"/></intent-filter>
    <meta-data android:name="org.androidaudioplugin.AudioPluginService.V4#Plugins"
        android:resource="@xml/aap_metadata"/>
    <meta-data android:name="org.androidaudioplugin.AudioPluginService.V4#Extensions"
        android:value="org.androidaudioplugin.juce.JuceAudioPluginServiceExtension"/>
  </service>
  <service android:name="org.androidaudioplugin.AudioPluginViewService"
      android:process=":adlplug" android:exported="true"/>

  <!-- secondary process -->
  <service android:name="org.androidaudioplugin.ports.juce.adlplug_ae.OpnplugAudioPluginService"
      android:process=":opnplug" android:label="AAP OPNplug-AE"
      android:foregroundServiceType="mediaPlayback" android:exported="true">
    <intent-filter><action android:name="org.androidaudioplugin.AudioPluginService.V4"/></intent-filter>
    <meta-data android:name="org.androidaudioplugin.AudioPluginService.V4#SecondaryPlugins"
        android:resource="@xml/aap_metadata_opnplug"/>
    <meta-data android:name="org.androidaudioplugin.AudioPluginService.V4#Extensions"
        android:value="org.androidaudioplugin.juce.JuceAudioPluginServiceExtension"/>
    <meta-data android:name="org.androidaudioplugin.AudioPluginService.V4#ViewService"
        android:value="org.androidaudioplugin.ports.juce.adlplug_ae.OpnplugAudioPluginViewService"/>
  </service>
  <service android:name="org.androidaudioplugin.ports.juce.adlplug_ae.OpnplugAudioPluginViewService"
      android:process=":opnplug" android:exported="true"/>

  <!-- stock MIDI device services, main process, one XML each (see below) -->
  <service android:name="org.androidaudioplugin.midideviceservice.StandaloneAudioPluginMidiDeviceService" ...>
    <meta-data android:name="android.media.midi.MidiDeviceService" android:resource="@xml/midi_device_info"/>
  </service>
  <service android:name="org.androidaudioplugin.midideviceservice.StandaloneAudioPluginMidiUmpDeviceService" ...>
    <property android:name="android.media.midi.MidiUmpDeviceService" android:resource="@xml/ump_device_info"/>
  </service>
</application>
```

The app needs no `Application` subclass. JUCE is initialised per process through the
`#Extensions` hook (see [aap-juce changes](#aap-juce-changes)).

## Metadata changes

### One `aap_metadata.xml` per service (`#Plugins` / `#SecondaryPlugins`)

*(Revised; the plan first had one shared file with a `process` attribute on each `<plugin>`. See
Status.)*

Each AudioPluginService has its own `aap_metadata.xml`, listing only the plugins it hosts:

- **The primary service** (stock `org.androidaudioplugin.AudioPluginService`) declares its file
  as the usual `org.androidaudioplugin.AudioPluginService.V4#Plugins` meta-data.
- **The other services** declare theirs as the new
  `org.androidaudioplugin.AudioPluginService.V4#SecondaryPlugins` meta-data, with the same format.
  aap-core reads either key. Hosts built with aap-core 0.11.1 or earlier only know `#Plugins`, so
  they ignore these services ("found, but with no readable AAP metadata XML resource"): they
  support only one service per package and would block on the plugins of the others.

### `#ViewService` service meta-data

This is a new key, `org.androidaudioplugin.AudioPluginService.V4#ViewService`, named like the
existing `#Plugins` and `#Extensions` keys.

- **Where it goes.** It is set on an `AudioPluginService` declaration, and its value is the
  fully-qualified class name of the view service in the same process.
- **Why fully-qualified.** Meta-data strings are not rewritten like manifest `android:name`
  values, and `namespace` can differ from `applicationId`, so a relative `.Foo` value is not
  supported.
- **When it's absent.** The stock `org.androidaudioplugin.AudioPluginViewService` is used, which
  is today's behaviour.

### `plugin-id` attribute on MIDI device ports

These are platform facts, taken from AOSP's `MidiService` and `MidiDeviceService`:

- **One service component effectively serves one MIDI device.** Several sibling `<device>`
  elements are all registered. But `MidiDeviceService.onCreate()` takes
  `getServiceDeviceInfo(package, class)`, which returns the first match, and every device binds
  the same component.
- **On port elements, only `name` is kept.** This applies to `<input-port>`, `<output-port>` and
  UMP `<port>`. The attribute loop matches on the local name. Unknown attributes on `<device>` go
  into `MidiDeviceInfo.getProperties()`.

So a package with several plugins exposes one MIDI device with one port per plugin (the
aap-lv2-mda model). The mapping from a port to a plugin is explicit:

```xml
<devices xmlns:aap="urn:org.androidaudioplugin.core">
  <device name="ADLplug-AE / OPNplug-AE" manufacturer="androidaudioplugin.org" product="ADLplug-AE">
    <input-port name="ADLplug-AE" aap:plugin-id="juceaap:adlplug-ae"/>
    <input-port name="OPNplug-AE" aap:plugin-id="juceaap:opnplug-ae"/>
  </device>
</devices>
```

- **How AAP gets the attribute.** The platform ignores it, so AAP reads the XML itself (see
  [aap-core change 7](#7-midi-device-service-port-mapping)).
- **The UMP variant.** The UMP XML (`<port>` elements) uses the same attribute.
- **Naming rule.** The attribute must never be called `name`, in any namespace, because the
  platform's port parser matches local names.

## aap-core changes

### 1. Discovery and metadata parsing

- **`PluginServiceInformation`:** add `processName` (from `ServiceInfo.processName`) and
  `viewServiceClassName` (from `#ViewService`, nullable).
- **`AudioPluginHostHelper`:**
  - Add `AAP_METADATA_NAME_VIEW_SERVICE = "$AAP_ACTION_NAME#ViewService"`.
  - `createAudioPluginServiceInformationWithDiagnostics()` reads the new key, records
    `serviceInfo.processName`, and reads the plugin metadata from `#Plugins`, or from
    `#SecondaryPlugins` if the service has no `#Plugins`.

`parseAapMetadata()` is the only aap_metadata parser. Native code gets plugin information from it
through JNI (`AAPJniFacade::queryInstalledPluginsJNI()`).

### 2. Looking up the plugin package's own services

`AudioPluginServiceHelper.getLocalAudioPluginService(context)` returns
`.first { svc -> svc.packageName == context.packageName }`, which breaks as soon as a package has
two services.

- Add to `AudioPluginServiceHelper`:
  - `getLocalAudioPluginServices(context)`: every AAP service of `context.packageName`.
  - `getLocalAudioPluginService(context, serviceClassName)`: one specific service.
  - `findLocalPluginInformation(context, pluginId)`: a plugin, whichever service hosts it.
- Deprecate `getLocalAudioPluginService(context)`. It keeps returning the first service.
- Callers to update:

| Caller | Change |
|---|---|
| `AudioPluginService.onCreate()` (extension list) | use its own component, `ComponentName(this, javaClass)` |
| `AudioPluginServiceHelper.createNativeViewFactory()` | `findLocalPluginInformation()` |
| `StandaloneAudioPluginMidiDeviceService`, `StandaloneAudioPluginMidiUmpDeviceService` | plugins of all local services |
| `LocalPluginManagerMain()` (ui-compose-app, `GenericPluginManagerMain.kt`) | all local services |
| `ComposePluginView` (ui-compose) | `findLocalPluginInformation()` |
| `AudioPluginWebViewFactory` (ui-web) | `findLocalPluginInformation()` |
| `AudioPluginServiceTesting` (testing) | iterate all local services; connect to each plugin's own service |

The `AudioPluginService.Extension` KDoc also names the key as
`org.androidaudioplugin.AudioPluginService#Extensions`, without `.V4`. Fix it at the same time.

### 3. Host connections keyed by service component (Kotlin)

- **`AudioPluginClientBase`:**
  - Add `connectToPluginService(packageName, className)`. The package-only overload stays with
    today's behaviour (first service in the package) and is deprecated.
  - `instantiateNativePlugin(pluginInfo)` looks up the connection by
    `(packageName, localName)`. `PluginInformation.localName` is already the service class name.
  - Add `disconnectPluginService(packageName, className)`, and make `dispose()` unbind each
    connection by its own component.
- **`AudioPluginServiceConnector`:**
  - Add `unbindAudioPluginService(packageName, className)`.
  - Add a class name to `nativeOnServiceConnectedCallback()`.
- **`AudioPluginHostHelper`:**
  - `ensureBinderConnected(packageName, className, connector)` (called from JNI).
  - `ensureBinderConnected(service, connector)` checks for an existing connection by component,
    not by package.
  - Deprecate `queryAudioPluginService(context, packageName)` (`.first()`).
- **Callers:**
  - `PluginManagerScope.instantiatePlugin()` (ui-compose-app)
  - `AudioPluginMidiDeviceInstance.create()` (midi-device-service)
  - `AapValidatorReceiver` (samples/aaphostsample)
  - `AudioPluginServiceTesting`
  - the usage example in the `AapAutomationRuntime` KDoc (js-controller)

### 4. Host connections keyed by service component (native)

`PluginClient::connectToPluginService(packageName, className, callback)` already has the class
name, but drops it when it calls `PluginClientSystem::ensurePluginServiceConnected()`, which only
takes a package name. Java then binds the package's first service.

- **`PluginClientSystem`** (`include/aap/core/host/plugin-client-system.h`): add
  `ensurePluginServiceConnected(connections, packageName, className, callback)`. Keep the
  package-only overload for source compatibility; aap-juce and aap-clap-hosting-helper call it.
  It keeps today's behaviour.
- **`AndroidPluginClientSystem`**
  (`androidaudioplugin/src/main/cpp/android/audio-plugin-host-android-internal.{h,cpp}`):
  implement the new overload.
- **`PluginClient::connectToPluginService()`**
  (`androidaudioplugin/src/main/cpp/core/hosting/PluginHost.Client.cpp`): pass the class name
  through.
- **`AAPJniFacade::ensureServiceConnectedFromJni()`**
  (`androidaudioplugin/src/main/cpp/android/AAPJniFacade.cpp`):
  - Take the class name and call the new `ensureBinderConnected` JNI signature.
  - Key `inProgressCallbacks` by `package/class`. Today it is keyed by package, so two services
    of one package connecting at the same time overwrite each other's callback.
  - A legacy package-only request is registered with an empty class name. When the connection
    arrives, it is matched as a fallback.
- **`AAPJniFacade::handleServiceConnectedCallback()`** and the JNI entry point for
  `AudioPluginServiceConnector.nativeOnServiceConnectedCallback`
  (`AudioPluginNatives_jni.cpp`): take the class name.

Already correct: `PluginClientConnectionList` (keyed by package and class) and
`addBinderForClient` / `removeBinderForClient`.

### 5. Plugin UI routing

- **`AudioPluginViewService`:** make it `open`. Subclasses must not need to override anything.
- **`AudioPluginSurfaceControlClient.bindPluginViewService(pluginPackageName)`:** it becomes
  `bindPluginViewService(pluginPackageName, pluginId)`. It finds the service in the package that
  lists `pluginId`, and binds `viewServiceClassName ?: AudioPluginViewService::class.java.name`.
  - The internal callers (`connectUI*`, `getPreferredSizeNoHandler`) already have `pluginId`.
  - Cache the result per `(package, pluginId)`.
- **Unchanged:** the public signatures (`connectUI`, `connectUIAsync`, `connectUINoHandler`,
  `getPreferredSize*`). So `GuiHelper.NativeEmbeddedSurfaceControlHost` and the JNI call to
  `connectUIAsync` in `AAPJniFacade` are unaffected.
- **Log messages:** the ones that print `AudioPluginViewService::class.java.name` should print
  the class actually bound.

### 6. Guard against misrouted instantiation in the plugin service

`AudioPluginInterfaceImpl` (`androidaudioplugin/src/main/cpp/android/AudioPluginInterfaceImpl.h`)
gets its plugin list from `PluginListSnapshot::queryServices()`, which returns every installed
plugin. If a host asks the `:adlplug` service for `juceaap:opnplug-ae`, that process would
`dlopen()` OPNplug's library and end up with two JUCE runtimes. Older hosts do exactly that,
because they bind the package's first service.

- `AudioPluginService.onBind()` passes its own class name:
  `AudioPluginNatives.createBinderForService(javaClass.name)`.
- `AudioPluginInterfaceImpl` keeps that name. Instantiation requests for plugins whose
  `localName` is a different class fail with a clear error ("plugin X is not hosted by service
  Y").

The guard runs inside the plugin's own process, so it protects multi-process plugins whatever
aap-core version the host was built with.

### 7. MIDI device service port mapping

- **Where the mapping comes from.** `AudioPluginMidiDevice` gets a lazily-read array of the plugin
  ID for each port. The array is read from the owning service's own XML:
  - MIDI 1.0: `ServiceInfo.loadXmlMetaData(pm, MidiDeviceService.SERVICE_INTERFACE)`.
  - UMP: `PackageManager.getProperty(MidiUmpDeviceService.SERVICE_INTERFACE, component).resourceId`,
    then `resources.getXml()`.

  `AudioPluginMidi1Device` / `AudioPluginMidi2Device` supply it, since they know their owner
  service.
- **Port index.** It is the order of the `<input-port>` elements (MIDI 1.0) or `<port>` elements
  (UMP).
- **Lookup.** `AudioPluginMidiDevice.getPluginId(portIndex)` uses `plugin-id` when it is present.
  Otherwise it falls back to today's device-name and port-name matching.

### 8. `isOutProcess` semantics

Today `isOutProcess = serviceInfo.packageName != context.packageName`. In a multi-process
package, a plugin in `:opnplug` is out of process as seen from the main process, even though the
package is the same.

- **The fix.** Compare `serviceInfo.processName` with the current process name,
  `Application.getProcessName()`. That needs API 28; minSdk is 29.
- **Current impact.** Nothing breaks today, because in-package callers pass
  `isRemoteExplicit = true`. The fix makes the flag stop lying.

### 9. Validator (`aapval`)

Add these checks to `samples/aaphostsample` (`AapValidatorReceiver`, `tools/aapval`):

- Every plugin is listed by exactly one AAP service in the package.
- In a package with more than one AAP service, only the primary one declares `#Plugins`; the
  others declare `#SecondaryPlugins`.
- At most one AAP service runs in each process.
- The `#ViewService` class is declared in the package, exported, and runs in the same process as
  its AAP service.
- A plugin with a native UI in a non-primary process has `#ViewService`. Without it, the UI would
  be routed to the stock view service in another process.
- Every MIDI port `plugin-id` refers to an existing plugin.

The validator's own connection code also moves to `(package, class)`.

### 10. Documentation

- `docs/DEVELOPERS.md`: the manifest and `aap_metadata.xml` sections (`#SecondaryPlugins`,
  `#ViewService`, multi-process packages).
- `docs/GUI_INTERNALS.md`: how the view service is found and bound.
- `docs/design/CLIENT_SERVICE.md`: connections keyed by service component.
- `docs/design/MIDI_DEVICE_SERVICE.md`: the platform facts above and `plugin-id`.
- `docs/schemas`: add the new attributes.

## aap-juce changes

- **New class `java/org/androidaudioplugin/juce/JuceAudioPluginServiceExtension.java`**, an
  `AudioPluginService.Extension`. Its `initialize(ctx)` (where `ctx` is the service):
  1. Finds this service's plugins, using change 2 and `ComponentName(ctx, ctx.getClass())`.
  2. Collects the distinct `library` values. If there is more than one, it throws: two JUCE
     runtimes cannot share a process.
  3. Calls `System.loadLibrary()` with the name stripped of `lib` and `.so`.
  4. Calls `com.rmsl.juce.Java.initialiseJUCE(ctx.getApplicationContext())`. This is the same
     call `JuceAppInitializer` makes.
  5. Keeps a process-wide "already initialised" flag, because the service can be destroyed and
     recreated in the same process. `cleanup()` does nothing.

  **Timing.** It runs in `AudioPluginService.onCreate()`, on the process's main thread, before
  `onBind()`. So no plugin instance or view exists in that process yet. That is the same
  guarantee `JuceAppInitializer` gives the main process. **No `Application` subclass** is needed,
  so the app's single `Application` slot stays free.
- **`com.rmsl.juce.Java` stays in each app.** Every port defines its own copy (aap-juce-byod,
  aap-juce-dexed, aap-juce-simple-host, aap-juce-template, ...) and compiles `aap-juce/java`, so
  shipping it from aap-juce would cause duplicate-class errors. A multi-process app must not load
  a library in its static initializer.
- **`JuceAppInitializer` is unchanged.** Single-process ports keep using it. A port uses either
  it or the extension, never both.
- **JUCE-based hosts (separate follow-up).** `AndroidAudioPluginFormat::createPluginInstance()`
  (`aap-modules/aap_audio_plugin_client/juceaap_audio_plugin_format.cpp`) passes
  `getPluginLocalName()` to the new `ensurePluginServiceConnected()` overload.
- **Docs.** `docs/JUCE_GUI_SUPPORT.md` should describe the multi-process variant of the
  manifest.

## aap-juce-adlplug-ae changes

- **Gradle and build files:**
  - Merge the `adlplug-ae/` and `opnplug-ae/` modules into one `app` module and update
    `settings.gradle`.
  - In the `Makefile`, drop `APP_MODULE_DIRS` so aap-juce's default `app` makes `dist` and CI work
    unchanged, and adjust `APP_NAME`.
  - The CMake setup already builds both libraries and doesn't change.
- **Resources:**
  - `aap_metadata.xml` (ADLplug-AE, `#Plugins`) and `aap_metadata_opnplug.xml` (OPNplug-AE,
    `#SecondaryPlugins`)
  - one `midi_device_info.xml` and one `ump_device_info.xml` (one device, two ports with
    `plugin-id`)
  - a label for each service
  - one launcher icon
- **Manifest:** as in [Target layout](#manifest-aap-juce-adlplug-ae-abridged). Remove the
  `JuceAppInitializer` meta-data. `JuceActivity` stays declared and is never launched.
- **Code:**
  - `Java.kt` loses its static `System.loadLibrary()`.
  - Add `OpnplugAudioPluginService` and `OpnplugAudioPluginViewService`, both empty subclasses.

  There is no `Application` subclass and there are no MIDI service subclasses.
- **Dependencies:** it needs an aap-core release that contains changes 1 to 7. During
  development, use `publishToMavenLocal`; `mavenLocal()` is already a repository.
- **README:** replace the paragraph about building two APKs.

## Other repositories

None of these is needed to merge ADLplug-AE and OPNplug-AE. But a host only reaches plugins in
non-primary processes once it has been updated and rebuilt against the new aap-core.

- **aap-clap-hosting-helper:** move `aap_clap_adapter.cpp`, `aap_clap_factory.cpp` and
  `aap_clap_preset_discovery.cpp` to the new `ensurePluginServiceConnected()` overload.
- **greenhouse:**
  - `AapHostEngine.kt`: switch to `connectToPluginService(packageName, className)`.
  - `PluginRepository.kt`: switch from `getLocalAudioPluginService()` to
    `getLocalAudioPluginServices()`.
- **aap-lv2:** `AudioPluginLV2ServiceExtension.initialize()` builds
  `ComponentName(context, AudioPluginService::class.java)`. It should use the service's actual
  class, so that it works in subclasses.
- **aap-lv2-sfizz:** `SfizzActivity` and its tests use the deprecated
  `getLocalAudioPluginService()`. That's fine while it has a single service, but it should
  migrate.

## Compatibility

| Host \ plugin | Existing single-service plugin | Multi-process plugin |
|---|---|---|
| Host on new aap-core | unchanged | full support |
| Host on old aap-core | unchanged | primary process only; see below |

What happens when an **old host** meets a **multi-process plugin**:

- **Discovery lists only the primary service.** Old aap-core does not know `#SecondaryPlugins`,
  so it ignores the other services, and old hosts see only the primary service's plugins, once.
  (With the original shared-file design, old hosts listed every plugin once per service, which
  crashed greenhouse.)
- **Binding is package-only**, and reaches the only service they see: the primary one.
- **They never see the plugins of the other services**, so they cannot block on them.
- **UI** binds the stock `AudioPluginViewService`, which runs in the primary process.

Plugins must be built with the new aap-core to use multiple processes: the view service must be
`open`, the extension lookup must be fixed, and the guard must be present.

## Rollout order

1. aap-core changes 1 to 7, then 8 to 10; release.
2. aap-juce `JuceAudioPluginServiceExtension`.
3. aap-juce-adlplug-ae restructure.
4. Host updates (aap-juce host module, aap-clap-hosting-helper, greenhouse), then the smaller
   follow-ups (aap-lv2, aap-lv2-sfizz).

## Testing

- **aap-core unit tests.** There are no tests for metadata parsing yet. Add them for:
  - the primary service selection
  - the MIDI `plugin-id` port mapping
- **Validator.** Run `aapval` on the merged APK.
- **On a device, with the merged APK:**
  - `adb shell ps -A | grep plug` shows the main process, `:adlplug` and `:opnplug`.
  - The in-app manager lists both plugins, and each one instantiates, plays and shows its JUCE
    UI.
  - The MIDI device shows two ports, and each routes to the right plugin.
  - An external host (aaphostsample, or greenhouse after its update) instantiates both plugins
    and shows their UIs.
  - After `kill` on the `:opnplug` process, ADLplug-AE keeps playing.
  - A host built with the old aap-core (0.11.1): ADLplug-AE works, and OPNplug-AE fails with a
    clear error rather than crashing.

## Open decisions

- **Application ID of the merged app.** Keeping `org.androidaudioplugin.ports.juce.adlplug_ae`
  upgrades existing ADLplug-AE installs in place, but leaves OPNplug-AE installs as a separate old
  app. `versionCode` must be higher than what is already published.
- **Names.** The `#SecondaryPlugins` and `#ViewService` keys, and the `plugin-id` attribute and
  its namespace.
