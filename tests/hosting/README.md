# Service binding regressions

Run `./tests/hosting/run-bindings.py` from the repository root. Requires Python 3, a JDK with JNI headers, a C compiler, and cached Kotlin compiler/runtime, reflection, and coroutines jars matching `gradle/libs.versions.toml`. The runner uses those existing dependencies directly and cleans temporary binaries. It does not edit Gradle configuration, package Android native assets, install an APK, or require a device.

The tests compile the actual `AudioPluginServiceConnector.kt` and `PluginServiceConnection.kt`. Android service/Binder behavior and AAP native registration are controlled doubles; a tiny host JNI stub handles the native connected notification. These are connector lifecycle tests, not Android Binder integration tests or confirmation of the Samsung MARs report.

The first patch covers null/disconnected/dead bindings, false/throwing platform binds, connector closure, synchronous callback delivery before `bindService` returns, late callbacks during retries, registration failures, reentrant listener invalidation/closure, and concurrent terminal events. Completion and platform unbinding are claimed once per attempt; stale attempts cannot clean up a new attempt.

For API verification, run `./tests/hosting/run-bindings.py --api-baseline 3b4f8239`. It compares public Kotlin metadata and JVM descriptors; compiler-generated private accessors are excluded. The public SDK, JNI/AIDL interfaces, and Gradle configuration remain unchanged.
