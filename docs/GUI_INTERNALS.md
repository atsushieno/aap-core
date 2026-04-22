# GUI Internals

This document describes the current GUI communication model, as established by the latest native-view work (primarily for aap-juce). It is primarily an internal note for our own maintenance, not a formal plugin-host/plugin specification (but we are not supposed to change much for backward compatibility).

It intentionally separates:

- the latest implementation that actually runs today,
- the public API surface that external code can see,
- and older/staler design assumptions that still exist in comments and headers.

## Scope

This document covers:

- native Android `View` hosting via `AudioPluginViewService` + `SurfaceControlViewHost`
- the Kotlin plugin-host helper path centered around `AudioPluginSurfaceControlClient`
- how view factories are discovered and instantiated
- current viewport and preferred-size negotiation
- the status of the public GUI API (`aap/ext/gui.h`, plugin-host metadata, `NativeRemotePluginInstance`)

This document does not try to explain Web UI in depth.

## Terminology

- **plugin instance**: the audio processor instance, identified by AAP `instanceId`
- **service-side GUI controller**: one `AudioPluginViewService.Controller` created for one plugin instance UI session
- **service-side guiInstanceId**: a monotonically increasing integer allocated by `AudioPluginViewService` per connection session
- **plugin-host surface view**: the plugin-host-owned `SurfaceView` that receives the child `SurfacePackage`
- **plugin-side view host**: the `SurfaceControlViewHost` created inside `AudioPluginViewService`
- **content size**: the plugin UI's own preferred/native size
- **viewport size**: the currently visible region that the plugin host wants to expose

## High-level architecture

For native Android `View` hosting on API 30+:

1. The plugin host creates an `AudioPluginSurfaceControlClient`.
2. The client owns a `SurfaceView` (`AudioPluginSurfaceView`).
3. The client binds the plugin app's `AudioPluginViewService`.
4. The client sends a Messenger request containing:
   - plugin host token
   - display id
   - plugin id
   - plugin instance id
   - initial width/height
5. `AudioPluginViewService` creates a service-side controller for that plugin instance.
6. The controller constructs a `SurfaceControlViewHost` and creates the plugin's `View` by calling the plugin's `AudioPluginViewFactory`.
7. The controller wraps the plugin view in a clipping `FrameLayout` viewport.
8. The controller returns a `SurfaceControlViewHost.SurfacePackage` to the plugin host via Messenger reply.
9. The plugin host attaches that `SurfacePackage` to its `SurfaceView` using `setChildSurfacePackage()`.
10. Later size/scroll updates are sent as Messenger messages.

The important implementation fact is:

- **native remote UI is currently orchestrated almost entirely on the Kotlin/Android side, not through the C GUI extension API.**

That is the biggest practical difference from the older design notes.

## Current public API surface

### Plugin metadata

AAP metadata still exposes GUI-related fields in `aap_metadata.xml`:

- `gui:ui-view-factory`
- `gui:ui-activity`
- `gui:ui-web`

The namespace used by plugin-host-side metadata parsing is:

- `urn://androidaudioplugin.org/extensions/gui`

In current code, native embedded UI hosting depends primarily on:

- `gui:ui-view-factory`

This is parsed in `AudioPluginHostHelper.parseAapMetadata()` and stored in `PluginInformation.uiViewFactory`.

### Plugin-side Java/Kotlin API

The plugin must provide:

- `org.androidaudioplugin.AudioPluginViewService` as an exported service in `AndroidManifest.xml`
- an `AudioPluginViewFactory` implementation referenced by `gui:ui-view-factory`

`AudioPluginViewFactory` currently has this public contract:

- `open fun getPreferredSize(context, pluginId, instanceId): Size? = null`
- `abstract fun createView(context, pluginId, instanceId): View`

The important current meaning is:

- `getPreferredSize()` returns the plugin content's preferred Android pixel size, before plugin-host decoration
- `createView()` must return the actual root `View` to embed into the service-side `SurfaceControlViewHost`

### Plugin-host-side Java/Kotlin API

Current public Kotlin entry points are:

- `AudioPluginHostHelper.createSurfaceControl(context)`
- `AudioPluginSurfaceControlClient`

The client exposes:

- `surfaceView`
- `connectUI()` / `connectUINoHandler()` / `connectUIAsync()`
- `getPreferredSize()` / `getPreferredSizeNoHandler()`
- `resizeUI()`
- `configureViewport()`
- `close()`

### Native / JNI visible API

`NativeRemotePluginInstance` still exposes GUI methods:

- `createGui()`
- `showGui()`
- `hideGui()`
- `resizeGui()`
- `destroyGui()`

And the C extension surface still exists in:

- `include/aap/ext/gui.h`
- `include/aap/core/aapxs/gui-aapxs.h`

However, for the current native embedded `View` path, these APIs are no longer the main control plane.

## Public API status summary

### What is current and meaningful

These parts reflect the code path that is actually used today:

- `AudioPluginViewFactory`
- `AudioPluginViewService`
- `AudioPluginSurfaceControlClient`
- metadata field `gui:ui-view-factory`
- optional preferred-size negotiation through `AudioPluginViewFactory.getPreferredSize()`
- viewport configuration through Messenger opcodes

### What is still public but semantically stale

The comments and implied model in `aap/ext/gui.h` are stale relative to the current implementation.

Examples:

- comments talk about `create()/show()/hide()/resize()/destroy()` as if the plugin GUI were managed as an overlay window through the extension
- comments mention `WindowManager.addView()` / `removeView()` / `updateViewLayout()`
- comments imply the plugin-side GUI extension is the main runtime control path

That is no longer a good description of the embedded native-view flow.

The modern embedded flow is instead:

- plugin host binds `AudioPluginViewService`
- plugin host and service exchange Messenger messages
- service instantiates the plugin's view factory directly
- plugin host controls viewport and attachment through `SurfaceControlViewHost`

### Practical status of the C GUI extension

Treat the C GUI extension API as:

- still part of the exposed API surface
- still present in JNI/native bindings
- but **not the authoritative description of embedded remote native-view hosting anymore**

If a future cleanup happens, the headers/comments in `aap/ext/gui.h` should be rewritten to avoid describing the old overlay/window-manager model as if it were the current truth.

## The latest established internal protocol

This section describes the real plugin host <-> `AudioPluginViewService` protocol.

This protocol is internal.

It is not a compatibility promise for external plugin hosts. It is the current implementation contract inside `aap-core`.

## Transport

Transport is Android Binder + `Messenger` messages.

The plugin host binds directly to:

- `org.androidaudioplugin.AudioPluginViewService`

The plugin host sends requests through the service binder's `Messenger`.

The service replies either:

- through `Message.replyTo` (for `CONNECT`, `GET_PREFERRED_SIZE`)
- or not at all (for resize/viewport/disconnect)

## Message opcodes

Defined in `AudioPluginViewService`:

- `OPCODE_CONNECT = 0`
- `OPCODE_DISCONNECT = 1`
- `OPCODE_RESIZE = 2`
- `OPCODE_CONFIGURE_VIEWPORT = 3`
- `OPCODE_GET_PREFERRED_SIZE = 4`

### Shared request keys

- `MESSAGE_KEY_OPCODE = "opcode"`
- `MESSAGE_KEY_PLUGIN_ID = "pluginId"`
- `MESSAGE_KEY_INSTANCE_ID = "instanceId"`

### CONNECT request keys

- `MESSAGE_KEY_HOST_TOKEN = "hostToken"`
- `MESSAGE_KEY_DISPLAY_ID = "displayId"`
- `MESSAGE_KEY_WIDTH = "width"`
- `MESSAGE_KEY_HEIGHT = "height"`

### CONFIGURE_VIEWPORT request keys

- `MESSAGE_KEY_VIEWPORT_WIDTH = "viewportWidth"`
- `MESSAGE_KEY_VIEWPORT_HEIGHT = "viewportHeight"`
- `MESSAGE_KEY_CONTENT_WIDTH = "contentWidth"`
- `MESSAGE_KEY_CONTENT_HEIGHT = "contentHeight"`
- `MESSAGE_KEY_SCROLL_X = "scrollX"`
- `MESSAGE_KEY_SCROLL_Y = "scrollY"`

### Reply keys

- `MESSAGE_KEY_GUI_INSTANCE_ID = "guiInstanceId"`
- `MESSAGE_KEY_SURFACE_PACKAGE = "surfacePackage"`
- `MESSAGE_KEY_PREFERRED_WIDTH = "preferredWidth"`
- `MESSAGE_KEY_PREFERRED_HEIGHT = "preferredHeight"`

## Plugin-host-to-service flows

### 1. Preferred size query

Used before connection when the plugin host wants to know the plugin content's preferred size.

Plugin-host steps:

1. bind service
2. send `OPCODE_GET_PREFERRED_SIZE` with:
   - `pluginId`
   - `instanceId`
   - `replyTo`
3. await reply
4. unbind service

Service steps:

1. call `AudioPluginServiceHelper.getNativeViewPreferredSize(context, pluginId, instanceId)`
2. reply with `preferredWidth` / `preferredHeight`

Important notes:

- this is a one-shot query
- if no preferred size is available, reply is `(0, 0)`
- this path does **not** create a controller or a `SurfaceControlViewHost`

### 2. Initial connection

Plugin-host steps:

1. ensure plugin-host `SurfaceView` exists
2. wait until layout params exist
3. bind `AudioPluginViewService`
4. wait until the `SurfaceView` is attached to a display so `displayId` is valid
5. send `OPCODE_CONNECT` with:
   - `pluginHostToken = surface.hostToken`
   - `displayId = surface.display.displayId`
   - `pluginId`
   - `instanceId`
   - `width`
   - `height`
   - `replyTo = incomingMessenger`

Service steps:

1. allocate a fresh `guiInstanceId`
2. if another controller already exists for the same `instanceId`, close it first
3. create a new `Controller`
4. enqueue initialization on the service main looper via `queue.addIdleHandler`
5. inside initialization:
   - obtain `Display` from `DisplayManager`
   - create `SurfaceControlViewHost(service, display, hostToken)`
   - instantiate plugin `View` via `AudioPluginServiceHelper.createNativeView(service, pluginId, instanceId)`
   - create clipping `FrameLayout` viewport
   - add plugin view to viewport with initial `width` / `height`
   - call `setView(viewport, width, height)` on the plugin-side view host
   - reply with `guiInstanceId` and `surfacePackage`

Plugin-host completion:

1. receive reply on `incomingMessenger`
2. store `connectedGuiInstanceId`
3. release any previous `SurfacePackage`
4. call `surface.setChildSurfacePackage(surfacePackage)`
5. if a viewport configuration had already been queued, send it now

### 3. Resize

Plugin host sends `OPCODE_RESIZE` with:

- `instanceId`
- `width`
- `height`

Service behavior:

- `resize()` is implemented as a degenerate viewport configuration:
  - viewport = content = requested size
  - scroll = 0,0

So in practice resize means:

- relayout plugin-host-side surface package to requested size
- size content to requested size
- clear scroll offset

### 4. Viewport configuration

Plugin host sends `OPCODE_CONFIGURE_VIEWPORT` with:

- `instanceId`
- `viewportWidth`
- `viewportHeight`
- `contentWidth`
- `contentHeight`
- `scrollX`
- `scrollY`

Service behavior:

1. `viewHost.relayout(viewportWidth, viewportHeight)`
2. if plugin view layout params differ from `contentWidth` / `contentHeight`, replace them
3. apply translation:
   - `view.translationX = -scrollX`
   - `view.translationY = -scrollY`
4. invalidate viewport container

Important interpretation:

- the service does **not** implement scrollbars
- the service does **not** own plugin-host window decoration
- the service only exposes a clipped translated content view inside a viewport
- the plugin host decides how to compute viewport/content/scroll state

### 5. Disconnect

Plugin host sends `OPCODE_DISCONNECT` with:

- `pluginId`
- `instanceId`
- `guiInstanceId`

Service behavior:

1. find controller by `instanceId`
2. if not found, ignore
3. if `guiInstanceId` is stale, ignore
4. otherwise close controller and remove it from map

The `guiInstanceId` check exists specifically to avoid tearing down a newly-created controller because of a delayed stale disconnect from an older session.

## Controller identity rules

`AudioPluginViewService` tracks controllers by **plugin instance id**:

- `controllers: MutableMap<Int, Controller>` keyed by `instanceId`

Additionally, each controller gets a service-side `guiInstanceId`.

Current rules:

- at most one controller is kept alive per plugin `instanceId`
- reconnecting the same plugin instance replaces the previous controller
- `guiInstanceId` exists mainly for stale-disconnect filtering
- `guiInstanceId` is not the same thing as the C GUI extension's `aap_gui_instance_id`

That distinction matters.

The service-side `guiInstanceId` is an internal session token for Messenger protocol safety.

## View factory resolution

Service-side view creation always goes through `AudioPluginServiceHelper`.

Resolution steps:

1. `getLocalAudioPluginService(context)` finds the local plugin service metadata
2. the target `<plugin>` entry is found by `pluginId`
3. `pluginInfo.uiViewFactory` is read from `aap_metadata.xml`
4. class is loaded by reflection
5. class must derive from `AudioPluginViewFactory`
6. service constructs it with a no-arg constructor
7. service calls:
   - `getPreferredSize()` for size queries
   - `createView()` for actual UI instantiation

Important implementation detail:

- the service passes **its own service context** into the factory
- not a plugin-host activity context
- not a display context created on the service side for factory construction

This is the current established behavior that works with the latest JUCE/native-view integrations.

## View tree ownership currently installed by the service

When creating the plugin view, `AudioPluginViewService.Controller` sets:

- lifecycle owner on the plugin view
- saved state registry owner on the plugin view
- lifecycle owner on the viewport `FrameLayout`
- saved state registry owner on the viewport `FrameLayout`

The owners used are the `AudioPluginViewService` itself.

This is important for Compose-based UIs and other Android UI stacks that assume these owners exist.

## Plugin-host-side lifecycle rules that matter in practice

These are not pretty, but they are currently intentional and established.

### Idle-handler gating before connect

`AudioPluginSurfaceControlClient.connectUI()` uses idle handlers twice:

1. before setting initial layout params
2. before sending the connect message, until `surface.display != null`

This exists because connection cannot succeed correctly before:

- the plugin-host `SurfaceView` has layout params
- the plugin-host `SurfaceView` is attached to a live display

Without those preconditions, width/height or display routing can be wrong.

### Surface package replacement

When a new package arrives:

- any previous `SurfacePackage` is released
- the new package is attached immediately

This is part of reopen/reconnect stability.

### Pending viewport configuration

If the plugin host computes viewport state before the `SurfacePackage` is attached, it stores it locally and sends it after the surface package arrives.

This avoids a race where the plugin host already knows the viewport geometry but the remote surface is not ready yet.

### Reconnect policy on older Android versions

`alwaysReconnectSurfaceControl = Build.VERSION.SDK_INT <= TIRAMISU`

This is a plugin-host-side policy knob for reconnect behavior on older platform versions.

It is implementation detail, not public API.

## Current viewport model

The latest working model is:

- preferred size describes **plugin content** size
- plugin-host window decoration is plugin-host-owned
- the plugin host can choose a viewport smaller than content
- the plugin host can scroll by changing `scrollX` / `scrollY`
- service applies scrolling by translating the content view negatively inside a clipping parent

So the service-side geometry is:

- `SurfaceControlViewHost` size = viewport size
- viewport `FrameLayout` size = viewport size
- plugin content `View` size = content size
- plugin content translation = negative scroll offset

This is the current answer to:

- clipped large plugin UIs
- plugin-host-side title bar/window decoration
- plugin-host-side scrollbars
- preserving move/resize behavior without scrolling the title bar itself

## What the plugin host is responsible for

The plugin host currently owns all of the following policy decisions:

- whether to show native embedded UI at all
- fallback size if preferred size is absent
- max size clamping against available display/insets
- title bar / close button / move behavior
- scrollbars and scroll gestures
- mapping scrollbars/gesture state to `configureViewport()`
- deciding when a reconnect is needed
- hiding or destroying the `SurfaceView`

The service does **not** know about any of these higher-level plugin-host behaviors.

## What the service is responsible for

The service owns:

- finding the plugin's registered view factory
- instantiating the plugin `View`
- creating the `SurfaceControlViewHost`
- exposing a `SurfacePackage`
- maintaining one active controller per plugin instance
- applying viewport geometry requests
- ignoring stale disconnects through `guiInstanceId`

## Relationship to Compose plugin-host app behavior

The Compose sample plugin host currently uses this pattern:

1. ask preferred size first
2. use fallback width/height if preferred size is missing
3. create `SurfaceControlUIScope` with chosen content size
4. once `AndroidView` is attached, connect the surface-control client
5. compute viewport vs content geometry from plugin-host UI state
6. call `configureSurfaceGUIViewport()` whenever viewport/scroll changes

That is the current reference behavior for large native UIs.

## API level status

### Native embedded view hosting

- requires Android 11 / API 30 or later
- depends on `SurfaceControlViewHost`

### Earlier Android versions

For API < 30, native embedded remote `View` hosting is not available through this path.

In practice:

- Web UI remains the portable fallback
- metadata may still advertise GUI-related features, but native `SurfaceControlViewHost` embedding is not usable there

## Current practical guidance for maintainers

### If a plugin provides a native Android view today

The supported path is:

- implement `AudioPluginViewFactory`
- optionally implement `getPreferredSize()`
- return the real root `View` from `createView()`
- expose `AudioPluginViewService`
- set `gui:ui-view-factory` in `aap_metadata.xml`

### If a plugin host wants embedded native UI today

The supported path is:

- use `AudioPluginSurfaceControlClient`
- query preferred size first if useful
- treat plugin-host title bar/scrollbars/viewport as plugin-host concerns
- use `configureViewport()` for scrolling and clipping
- use `guiInstanceId` from service reply when disconnecting

### If editing the public GUI headers

Keep in mind:

- `aap/ext/gui.h` is exposed, but its comments currently describe an older model
- any attempt to "fix" the header comments should be careful not to claim semantics the runtime no longer uses
- if the API is kept, the comments should describe it as a legacy/general GUI extension surface, not as the exact control plane for `AudioPluginViewService`

## Mismatches with older documents

Compared to `docs/design/GUI.md`, the current code differs in a few important ways:

1. remote native-view hosting is now concretely Messenger-driven around `AudioPluginViewService`
2. preferred size is now an explicit query on `AudioPluginViewFactory`
3. viewport configuration is explicit and supports plugin-host-owned scrolling
4. the service instantiates the view factory directly with service context
5. the C GUI extension is no longer the best description of the remote native-view path

So when this document conflicts with the older design note, prefer this document for current maintenance work.

## Recommended future cleanup

The following would reduce confusion later:

1. update `aap/ext/gui.h` comments so they do not describe the old overlay/window-manager model as the primary truth
2. decide whether the Messenger protocol should eventually be formalized or remain internal
3. clarify naming so service-side Messenger `guiInstanceId` cannot be confused with extension-layer GUI ids
4. document Web UI separately from native embedded view hosting
5. move more of the current plugin-host-side viewport policy into a reusable internal helper if more hosts adopt the same pattern

## Quick reference

### Current canonical native embedded UI path

- Plugin host:
  - `AudioPluginHostHelper.createSurfaceControl()`
  - `AudioPluginSurfaceControlClient`
- Service:
  - `AudioPluginViewService`
- Factory:
  - `AudioPluginViewFactory`
- Metadata:
  - `gui:ui-view-factory`

### Current internal control messages

- `CONNECT`
- `DISCONNECT`
- `RESIZE`
- `CONFIGURE_VIEWPORT`
- `GET_PREFERRED_SIZE`

### Current geometry model

- plugin host controls viewport
- plugin factory controls content view creation
- service applies clipping + translation
- plugin host controls decoration and scrolling
