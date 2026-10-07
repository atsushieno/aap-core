# AAPXS instance lifecycle and ownership (AI slop)

An `AAPXSDefinition` describes an extension and may be shared by many plugin
instances. Store instance state in the initiator or recipient `aapxs_context`,
not in the definition. Each direction has its own instance and serialization
context. These SDK structs are in-process objects, not wire records.

## Setup and release

The dispatcher first maps all buffers and installs the framework's optional
services, then invokes `initialize_initiator_instance` and
`initialize_recipient_instance`. Their instance pointers remain stable through
release. `is_host_extension` describes the request's target: true for a
plugin-to-host request, false for a host-to-plugin request. Consequently, the
client's initiator is a plugin initiator, while the service's initiator is a host
initiator. Recipient roles are the opposite.

Creation runs on the control thread before proxies are exposed and before
processing starts. Return false to reject setup. Both dispatchers clean up
contexts created before a false return or exception. A failed service setup is
reported through the existing Binder creation error.

At teardown, stop/join request work and release the plugin first. The dispatcher
then calls `release_initiator_instance` / `release_recipient_instance` for each
non-null context, while its instance struct and buffers still exist. These
callbacks receive the role and full instance, so distinct context types need no
shared type tag. Release must not throw, access the released plugin through
`host_context`, or send new requests. The dispatcher clears the context afterward.
It also cleans partial setup when destroyed directly, without `PluginHost`.

The role-specific callbacks take precedence over the legacy
`release_instance_context` and plugin-recipient release callback. Legacy callbacks
remain available for source migration; they do not identify roles.

## Typed clients and proxies

`initializeTypedAAPXSInitiator<Client, Service>` creates the canonical typed
client for the relevant direction. It stores the owned object in `aapxs_context`
and advertises its `TypedAAPXS*` base pointer through `typed_client`.
`releaseTypedAAPXSInitiator` releases it through the virtual destructor. Omit
`Service` when there is no outgoing host extension.

Proxy getters return interfaces from that initialized context. They must not
create another client or mutate definition-owned proxy state. Hosting's
`ClientStandardExtensions` borrows the advertised standard clients; it never
owns or deletes them. An implementation with a different opaque context leaves
`typed_client` null unless it explicitly provides a compatible typed helper.
Standard C++ convenience methods report unavailable interfaces safely.

Typed requests keep their own payload/reply buffers. The transport serializes
access to shared Binder storage. Direct writes to the extension's shared
serialization buffer are not part of the typed-client calling contract.

## Framework services and cancellation

`host_context` is opaque to extensions and the typed helper. A framework may
provide borrowed immutable `plugin_id` metadata and an initiator
`request_metadata_refresh` callback for scheduling control-thread refresh work.
Standard implementations use these services instead of interpreting the opaque
framework pointer.

The optional `register_abort_handler` service returns an opaque registration.
The typed helper copies its paired `unregister_abort_handler` function, so it can
unregister even after the framework owner or initiator struct is destroyed.
The registration must retain the needed lifetime, exclude future abort delivery,
and wait for concurrent delivery when unregistered. Reentrant unregistration
from the handler itself is supported. A framework must also cancel/retire its
transport requests during shutdown; late transport completions must respect the
transport's completion contract.

The reference `AsyncAbortRegistry` is also usable outside `PluginInstance`.
Construct it with `std::make_shared`, attach it before initializing clients, and
invoke `abort(error)` during transport failure/teardown. It snapshots retained
subscriptions and excludes delivery from concurrent unregistration. All these
registration, cancellation, creation, and release operations belong on control
threads, never in `process()`.

## Host extension receiver

`get_host_extension_receiver` supplies the local native host-extension fallback
used to receive incoming requests. Framework built-ins and the host's own
implementation take precedence. It is distinct from `get_host_extension_proxy`,
which exposes an outgoing request sender inside the plugin service. Keep returned
interface pointers valid until instance teardown.

## SDK migration

This update changes the native SDK API/ABI. Rebuild all native libraries that use
these definitions, instance structs, typed helpers, or standard clients against
matching headers/libraries. A matching published version number alone is not
sufficient. No lifecycle pointer, role, helper pointer, or cancellation service
is serialized; peer-facing AIDL, opcodes, capacities, and extension records are
unchanged. Older plugin/host counterparts continue using their existing wire
protocol.
