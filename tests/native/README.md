# AAPXS regression tests

Run `./tests/native/run-aapxs-lifecycle.sh` from the repository root. Requires clang++ and UBSan. The runner builds the actual transport/session/typed implementations in debug and release configurations, running the tests present at each stage of the patch series. Temporary binaries are cleaned up. macOS gets a clock_nanosleep compatibility shim; vptr sanitization is disabled because this partial link does not provide PluginInstance RTTI.

Request ID zero is covered for reply delivery, coexistence with ID one, timeout, and capacity reuse. The session retains its public size, API, and 255-slot limit; occupied slots are identified by their callback pointer.

Typed completion removes the request from the client map before invoking user code, keeping the request buffer owned until delivery returns. A regression callback destroys its own typed client. Failure snapshots also match request ID and address before completing a call.

Lifecycle tests cover typed/session destruction, 400 successive cancellations, late replies, stale callback tokens, concurrent cancellation during reply copying, reentrant callbacks, and legacy unsolicited host requests. Session gates live in a private sidecar to preserve the public ABI. Registrations are removed before request storage is freed. Binder connection/abort-registry races and real-time allocation/locking remain outside this fix.

Saturation tests cover 255 session callbacks and 1,024 global requests, immediate rejection without encoding or Binder fallback, subsequent capacity reuse, and encoding failure releasing its registration. Limits and public signatures remain unchanged. Legacy calls without an error callback receive their completion callback as the fallback failure notification.

Parameter scans retain transport errors for every count/parameter/enumeration read and publish only a complete successful result. Tests inject errors and short replies at every read, including the final read after partial progress, and check invalid counts, valid empty layouts and bounded names. Existing public getters retain their defaults.

MIDI clients send the existing length + UTF-8 plugin-ID record for compatibility with older services. New services prefer their instance metadata so older clients with empty/stale payloads select the correct preference key. Tests cover bounds, legacy Binder unknown lengths, actual handlers/reply size, and typed client success/error/short replies; only the JNI preference lookup is stubbed. SharedPreferences and mixed-version processes are not tested on-device. No public layout, API, opcode or wire format changes.

Instantiation failures (#11): a missing Java connection or negative native creation result now uses the existing AudioPluginException instead of an assertion or an invalid wrapper. No public method, descriptor, layout, or wire record changes.
