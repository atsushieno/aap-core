# AAPXS regression tests

Run `./tests/native/run-aapxs-lifecycle.sh` from the repository root. Requires clang++ and UBSan. The runner builds the actual transport/session/typed implementations in debug and release configurations, running the tests present at each stage of the patch series. Temporary binaries are cleaned up. macOS gets a clock_nanosleep compatibility shim; vptr sanitization is disabled because this partial link does not provide PluginInstance RTTI.

Request ID zero is covered for reply delivery, coexistence with ID one, timeout, and capacity reuse. The session retains its public size, API, and 255-slot limit; occupied slots are identified by their callback pointer.

Typed completion removes the request from the client map before invoking user code, keeping the request buffer owned until delivery returns. A regression callback destroys its own typed client. Failure snapshots also match request ID and address before completing a call.
