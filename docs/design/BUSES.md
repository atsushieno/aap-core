# Audio and event buses (AI slop with minimum changes)

This document describes how AAP organizes plugin inputs and outputs as
**buses**, and how bus layouts are configured and changed at run time.

## Concepts

A **bus** is one logical connection. An audio bus carries one or more
channels; one channel used to be one "port".

| property | values | notes |
|---|---|---|
| `id` | uint32 | Stable across plugin versions. Hosts persist it for routing. It is not the position. |
| `kind` | `AAP_BUS_KIND_AUDIO`, `AAP_BUS_KIND_EVENT` | |
| `direction` | `AAP_PORT_DIRECTION_INPUT`, `AAP_PORT_DIRECTION_OUTPUT` | |
| `role` | `AAP_BUS_ROLE_MAIN`, `AAP_BUS_ROLE_AUX` | At most one main bus per (kind, direction), always at index 0. Aux covers sidechains and extra outputs. |
| `name` | string | |
| `channel_count` | int | Audio only. |
| `layout` | string tag | Audio only: `mono`, `stereo`, `lcr`, `quad`, `5.1`, `7.1`, `7.1.4`, `ambisonic(order)` or `discrete(n)`. |
| `flags` | bitset | `AAP_BUS_FLAG_OPTIONAL`, `AAP_BUS_FLAG_ENABLED_BY_DEFAULT`, `AAP_BUS_FLAG_NOTES`. |
| `enabled` | bool | A disabled bus has no buffers. |

**Index spaces.** Buses are indexed per (kind, direction), with the main bus
at index 0, as in VST3, CLAP and JUCE.

**Event buses.** Every plugin has a main event input and a main event output,
whether it is an instrument or an effect. The framework creates them, and
plugins do not report them. Their IDs are reserved:
`AAP_BUS_ID_MAIN_EVENT_INPUT` and `AAP_BUS_ID_MAIN_EVENT_OUTPUT`. They carry
UMP: notes, parameter changes and notifications, and AAPXS SysEx8. Aux event
buses are reserved for the future; use UMP groups meanwhile.

**Audio buffers.** Each audio bus has `channel_count` planar float buffers.
Channels are never interleaved.

## Where buses come from

Bus information is **not** in `aap_metadata.xml`. There is no `<bus>` element,
and `<port>` is no longer authored. The instance is the source of truth.

- **Plugins that implement the buses extension** report their audio buses and
  declare `urn://androidaudioplugin.org/extensions/buses/v1` in their
  metadata. `<extensions bom="0.12.1" />` declares it as well. BOM 0.12.1 is
  0.12.0 plus the buses extension.
- **Other plugins** get the category defaults from the framework:
  - an `Effect` has a stereo main input and a stereo main output;
  - an `Instrument` has a stereo main output;
  - both have the main event buses.

## Plugin API

### Reporting buses

A plugin returns an `aap_buses_extension_t` (`aap/ext/buses.h`) from
`get_extension()`:

- `get_bus_count(kind, direction)` and `get_bus(kind, direction, index)`
  report the **audio** buses only.
- `apply_layout(request)` is optional (may be NULL). The host calls it only
  when the plugin is not active. It accepts or rejects the whole layout
  atomically, and must leave the layout unchanged when it returns false.

`samples/aappluginsample/src/main/cpp/effect/aap-effect-sample.cpp` is an
example that switches between mono and stereo.

### Accessing buffers

`aap_buffer_t` has bus accessors. They exist only when
`host->get_extension(host, AAP_BUSES_EXTENSION_URI)` returns non-NULL:

- `get_bus_count(self, kind, direction)`;
- `get_audio_channel_count(self, direction, bus)`: the channels actually
  allocated, which is 0 for a disabled bus;
- `get_audio_channels(self, direction, bus)`: the channel buffers, or NULL;
- `get_event_buffer(self, direction, bus)`: starts with `AAPMidiBufferHeader`;
- `get_event_buffer_capacity(self, direction, bus)`.

Always take the channel count from the buffer, not from what the plugin
reported or accepted: until the host prepares again, the buffers keep the
previous layout. `prepare()` may be called again on an instance that was
prepared before (see "Re-prepare"), and plugins must re-read their buffers
then.

`num_ports()` and `get_buffer(index)` remain as a flattened view until 1.0:
audio input channels, audio output channels, event input, event output.

### Changing buses

To change its own buses (a patch or a preset with another layout), the plugin
calls `notify_buses_changed()` on the host extension
`aap_buses_host_extension_t`:

- `AAP_BUSES_CHANGED_NAMES`: only names changed. Allowed at any time.
- `AAP_BUSES_CHANGED_LAYOUT`: anything that affects buffers (bus count,
  channel counts, enabled buses).

It is realtime-safe. The plugin keeps processing with its current buffers
until the host prepares it again, which may never happen.

## Host API

C++ (`aap::RemotePluginInstance`):

- `getNumBuses(kind, direction)`, `getBus(kind, direction, index)`, and
  `getMainEventPortIndex(direction)`. `BusInformation::getPortIndex(channel)`
  maps a channel to its flattened port index.
- `applyBusLayout(request)`: requests a layout from the plugin when not
  active. On success the instance becomes UNPREPARED, and the host calls
  `prepare()` again.
- `setBusesChangedHandler(handler)`: receives `AAP_BUSES_CHANGED_*`. To follow a
  layout change, the host deactivates the instance if needed, then calls
  `refreshBusLayout()` and `prepare()` again.
  - Changes notified while no handler is set are delivered to the next handler
    as soon as it is set.
  - Clearing or replacing the handler waits for running calls to the previous
    one, so its owner can be destroyed right after clearing it.

Kotlin (`NativeRemotePluginInstance` and `AudioPluginInstance`): `getBusCount()`,
`getBus()`, `applyBusLayout(List<BusLayoutRequest>)`, `refreshBusLayout()` and
`setBusesChangedListener(IntConsumer?)`. `NativeRemotePluginInstance` also has
`getAudioPortIndices()` and `getMainEventPortIndex()`.

`androidaudioplugin-manager` already follows plugin-initiated layout changes,
keeping the event buffer size it prepared with.

## Wire protocol

No AIDL signature changes. Everything goes through the buses AAPXS.

### Bus mode and legacy mode

| host runtime | plugin | mode |
|---|---|---|
| new | declares the buses extension | bus mode |
| new | does not declare it (including older APKs) | legacy mode |
| old | any | legacy mode |

- **Client:** bus mode if the plugin declares the extension in its metadata.
  An older service is never sent a buses request.
- **Service:** bus mode once the client has asked for the layout. An older
  client never asks.
- **Legacy mode** is the previous behavior: both sides compute the same port
  list, from `<port>` or the category defaults, and register one buffer per
  port.

### Opcodes

| opcode | direction | handled by |
|---|---|---|
| `GET_LAYOUT` (1) | host → plugin | Framework. Returns all buses, including the event buses, with the layout generation. |
| `COMMIT_BUFFER_LAYOUT` (2) | host → plugin | Framework. Commits where each port's buffer is in the pool. |
| `APPLY_LAYOUT` (3) | host → plugin | The plugin's `apply_layout`. |
| `NOTIFY_BUS_NAMES_CHANGED` (-1) | plugin → host | Coalesced, payload-free. |
| `NOTIFY_BUS_LAYOUT_CHANGED` (-2) | plugin → host | Coalesced, payload-free. |

### Buffer pool

In bus mode, each instance has **one shared memory pool**:

1. The client reads the layout (`GET_LAYOUT`) and computes a buffer layout.
   It has one entry per flattened port, each with an offset and a size, and
   offsets are 64-byte aligned.
2. The client commits it (`COMMIT_BUFFER_LAYOUT`) with the generation it was
   based on. The service rejects it if the generation is stale or the
   instance is active.
3. `beginPrepare`, then `prepareMemory(instance, 0, poolFD)` with the only FD,
   then `endPrepare`. The service validates the layout against the actual
   pool: in bounds, no overlaps, large enough.

If the commit fails on the first prepare, the client falls back to per-port
buffers.

### Generation

The service increments the layout generation when `apply_layout` succeeds and
when the plugin notifies a layout change, but not on a name change. A buffer
layout based on an older generation is rejected, so a host that raced with a
plugin change simply reads the layout again.

### Re-prepare

An INACTIVE instance with a pool can be prepared again, with a **new pool**,
for a new layout or for a new frame count. The previous pool stays mapped
until the next swap. Legacy mode never re-prepares.

```
host                                 service
 |  deactivate (if ACTIVE)  ---------> plugin->deactivate()
 |  [host request] APPLY_LAYOUT -----> plugin->apply_layout(); generation++
 |  GET_LAYOUT ----------------------> {generation, buses}
 |  allocate the pool, compute the buffer layout
 |  COMMIT_BUFFER_LAYOUT ------------> generation and state checked
 |  beginPrepare / prepareMemory / endPrepare --> map the new pool, plugin->prepare()
 |  activate
```

## Format wrappers and hosts

| project | role | mapping |
|---|---|---|
| aap-juce plugin wrapper | plugin | Every JUCE bus is an AAP bus. `apply_layout` maps to `checkBusesLayoutSupported()` and `setBusesLayout()`. |
| aap-juce host format | host | Every AAP audio bus is a JUCE bus. A host layout change is requested from `canApplyBusesLayout()`. A plugin-initiated change is handled like VST3 `restartComponent(kIoChanged)`: on the message thread the instance is released, its JUCE buses follow the new layout, and it is prepared again. |
| aap-lv2 | plugin | LV2 port groups (`pg:mainInput`, `pg:mainOutput`, `pg:sideChainOf`) become buses; ungrouped ports become the main buses. No `apply_layout`. |
| aap-clap-hosting-helper | plugin | CLAP audio ports per bus. A plugin-initiated change is applied on the main thread: `requestRestart()` while active, then `audio-ports` rescan. |
| aap-cmajor | plugin | The loaded patch's ports. A patch with another layout notifies `LAYOUT`. |

## Until 1.0

These stay only for compatibility, and are removed at 1.0 together with the
`<parameters>` list:

- legacy mode and per-port buffer registration;
- the `<port>` reader;
- the flattened `num_ports()` / `get_buffer()` view.
