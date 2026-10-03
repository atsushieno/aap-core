# Processing and extension work

The managed `RemotePluginInstance::process()` and `LocalPluginInstance::process()` paths do not acquire application locks, including try-locks or sleeping/spinning locks. They do not allocate or reclaim C++ request storage. This covers core MIDI merging, AAPXS filtering/handoff, parameter value caching, GUI output publication, standard parameter/preset notifications and `request_process`.

Android Binder processing still uses its existing synchronous transaction. Binder scheduling, framework internals, tracing and plugin/host-supplied code are outside this application-level guarantee. Arbitrary plugin DSP and a host's custom extension callback must observe their own processing constraints. This change does not provide realtime priority inheritance through Binder.

## Handoffs

Instances allocate their queues and start an extension worker during AAPXS setup, before processing. A queue has 64 records and one consumer. Producers make at most four atomic reservation attempts; the consumer never waits for an unpublished record. Atomic operations used by processing are checked as always lock-free at compile time, including double parameter values on all four Android targets.

MIDI producers copy complete UMP sequences into queues. Processing merges records that fit the destination's current payload capacity, leaving excess records queued for later blocks. A record larger than the port's entire payload capacity is discarded. Partial UMP records are rejected. The ordinary input/output/GUI record capacity is the instance's configured event MIDI buffer size (8 KiB by default).

Processing validates and filters complete AAPXS SysEx8 messages, copying their existing wire bytes into a separate preallocated inbox. The worker parses those copies, invokes extension handlers, copies replies into owned request buffers and completes callbacks. Processing can immediately reuse its port buffer. Reply delivery, cancellation, pending tables, Binder channels and typed completion ownership continue using their existing synchronization on non-processing threads.

The inbox accepts records up to twice the existing 1 KiB AAPXS parser capacity plus the MIDI header, enough for its encoded UMP packets. A full inbox drops the wire message; an outstanding request then reaches the existing timeout/error path. Outgoing request handoff rejection immediately reports an error and releases its session/table registration. There is no inline Binder or callback fallback on processing threads. `RealtimeByteQueue::rejectedCount()` exposes queue rejection counts for native diagnostics.

Standard parameter/preset notifications use a coalescing atomic mask; `request_process` uses a coalescing flag. They do not compete for inbox capacity. Other payload-free host notifications use a bounded queue with copied URIs; excess notifications are dropped and counted. Notifications do not promise one IPC transaction per original invocation.

The worker polls every 1 ms; there is no semaphore wakeup or lazy initialization in processing. Replies and notifications gain scheduling latency, and a SysEx8 reply is normally returned in a later audio block. Timeouts also progress when audio has stopped. A blocked user callback blocks its extension worker, rather than processing; it can eventually cause handoff backpressure.

## Control exclusion

Control calls serialize off the processing thread. The control thread publishes suspension and waits for any previously entered DSP block to leave. Processing announces activity and checks suspension using lock-free atomics. A suspended/inactive local block clears its outputs and MIDI input instead of calling DSP or waiting. Its complete AAPXS requests are still copied to the worker inbox. Ordinary MIDI for that block is discarded; queued UI MIDI is retained until DSP resumes.

All worker/Binder plugin extension handlers run between DSP blocks. `is_command_rt_safe` continues selecting the existing SysEx8 transport at the initiator, but does not authorize concurrent access to plugin state. Parameter scans, local preset JNI controls and activation/deactivation use the same exclusion. “RT-safe” alone does not imply thread-safe concurrent access. Control operations can produce silent blocks, especially long state/preset operations.

## SDK and lifetime

General typed AAPXS queries remain non-realtime APIs. In an annotated processing scope, synchronous wrappers return their existing default value or `RT caller` error, and async wrappers return `-1` without accepting a request or invoking its callback. Payload-free host notifications use their separate handoff. Callers must not construct allocating callback captures or invoke other non-realtime APIs inside DSP just because a defensive refusal exists. `RealtimeScope` is implemented in the core library, so SDK consumers in other shared objects use the same annotation even with hidden symbol visibility. Its bounded atomic thread registry avoids cold TLS initialization and allocations. It supports 256 distinct annotated threads at once; exhaustion temporarily refuses general SDK queries on every thread until capacity returns.

Host and native plugin proxies are constructed before processing and retrieved from instance caches. Metadata/index snapshots stay immutable and alive until instance destruction. Atomic value cells are shared by stable ID across revisions, preserving changes made through an older snapshot during publication. Processing never takes layout/publication locks or a global sidecar lookup lock.

Teardown stops and joins the worker before releasing sessions, dispatchers and buffers. `PluginHost::destroyInstance()` called from a worker callback defers deletion to a lifecycle thread, allowing the current handler to return before the join. Plugin release runs while its host facade and extension contexts still exist. The host must stop making processing calls before destroying an instance; direct deletion from inside its own worker callback is not supported.

Native C++ API/ABI changes require affected SDK consumers within the same app to rebuild. Peer-facing C extension/AAPXS structs, AIDL, opcodes, payload formats and shared capacities are unchanged. Existing counterparts need no protocol update. Deferred plugin-side replies that outlive their original recipient request context remain the separate issue #3; this change does not repair that ownership contract.

## Validation

Run `tests/native/run-aapxs-lifecycle.sh` and `python3 tests/native/check-aapxs-wire-compatibility.py 9e17764f`.

The native suite runs debug/release UBSan regressions, including concurrent queues, saturation/reuse, copied legacy wire messages, cancellation before delayed delivery, parameter snapshot publication, actual local/remote processing, blocked worker completion, incoming control exclusion and worker-triggered teardown. The processing fixture traps on mutex/try-lock use and C++ allocation/reclamation; deliberate failing probes verify those guards. macOS uses an injected test-only dyld lock interposer to include libc++ mutex calls. Android UI/JNI and memory setup are substituted in the host fixture; processing, dispatch, transport, cache and queue implementations are production code.

The native Android core and JS controller builds are checked for arm64-v8a, armeabi-v7a, x86 and x86_64. These checks do not replace audio/device stress testing or mixed-version APK testing. No Gradle test assets or `build.gradle.kts` changes are required.
