package org.androidaudioplugin.js

import android.content.Context
import android.util.Log
import java.util.concurrent.Callable
import java.util.concurrent.Executors

/**
 * Embedded JavaScript automation controller for AAP host apps.
 *
 * This mirrors the testing facade that uapmd-app exposes on Android: a single in-process JS engine
 * (choc/QuickJS, built into this module's native library — no WebView, no Chrome dependency) that
 * exposes the AAP host vocabulary as `globalThis.aap.*`, driven from the host over
 * `adb shell am broadcast` (see [AapAutomationReceiver]).
 *
 * ## Provider setup (per app)
 *
 * Apps built on the AAP host API are the *provider*. During startup, after the app has its native
 * plugin client connected, wire the runtime up:
 *
 * ```kotlin
 * AapAutomationRuntime.bootstrap(context)
 * AapAutomationRuntime.attachNativeClient(nativePluginClient.native) // aap::PluginClient*
 * AapAutomationRuntime.setPluginCatalog(catalogJson)                 // optional discovery feed
 * ```
 *
 * The [AapAutomationReceiver] is contributed by this module's manifest, so adding the dependency is
 * enough to expose the broadcast entry points; only the wiring above is app-specific.
 *
 * Not thread-safe: the native context is single-threaded. All script execution goes through the
 * receiver's single-thread executor, and the native side serializes access.
 */
object AapAutomationRuntime {
    private const val LOG_TAG = "aap-js-controller"
    private const val API_JS_ASSET = "aap-api.js"

    @Volatile
    private var bootstrapped = false

    // The QuickJS context is thread-affine: it records its stack base at creation, so it MUST be
    // created and used on one and the same thread (otherwise every eval reports a bogus stack
    // overflow). All native context access is funneled onto this single dedicated thread, regardless
    // of whether the caller is the activity (main thread) or the broadcast receiver's executor.
    private val engine = Executors.newSingleThreadExecutor { r -> Thread(r, "aap-js-runtime") }
    private fun <T> onEngine(block: () -> T): T = engine.submit(Callable { block() }).get()

    init {
        System.loadLibrary("androidaudioplugin-js-controller")
    }

    /** Creates the JS context (if needed) and loads the embedded `aap-api.js` facade. Idempotent. */
    @Synchronized
    fun bootstrap(context: Context) {
        if (bootstrapped)
            return
        val source = context.applicationContext.assets.open(API_JS_ASSET).use {
            it.readBytes().toString(Charsets.UTF_8)
        }
        onEngine { nativeBootstrap(source) }
        bootstrapped = true
        Log.i(LOG_TAG, "AapAutomationRuntime bootstrapped")
    }

    /**
     * Hands the connected native client pointer (`aap::PluginClient*`) to the runtime.
     * Obtain it from `NativePluginClient.native`. Pass 0 to detach.
     */
    fun attachNativeClient(nativeClientHandle: Long) = onEngine { nativeSetClient(nativeClientHandle) }

    /** Optional: feed the JVM-side plugin discovery result as JSON for `aap.discovery.getPlugins()`. */
    fun setPluginCatalog(json: String) = onEngine { nativeSetPluginCatalog(json) }

    /**
     * Host-provided hook that binds a plugin's Android service by package name, synchronously, and
     * returns true on success. AAP must bind the plugin service before instancing, so the JS facade
     * (`aap.instancing.connect` / auto-connect in `aap.instancing.create`) calls into this.
     *
     * Wire it from the host, typically wrapping the suspend `AudioPluginClientBase.connectToPluginService`:
     * ```kotlin
     * AapAutomationRuntime.serviceConnector = { pkg -> runBlocking { client.connectToPluginService(pkg); true } }
     * ```
     */
    @JvmStatic
    var serviceConnector: ((String) -> Boolean)? = null

    /** Invoked from native (the `__aap_connect_service` binding) to bind a plugin service. */
    @JvmStatic
    fun connectService(packageName: String): Boolean {
        val connector = serviceConnector
        if (connector == null) {
            Log.w(LOG_TAG, "connectService($packageName) called but no serviceConnector is wired")
            return false
        }
        return try {
            connector(packageName)
        } catch (e: Throwable) {
            Log.e(LOG_TAG, "connectService($packageName) failed", e)
            false
        }
    }

    /** Runs a script synchronously, returning its result as JSON (or an `ERROR: ...` string). */
    fun runScript(code: String): String = onEngine { nativeRunScript(code) }

    /** Starts a script as an async job; returns the job id to poll with [getJob]. */
    fun startJob(code: String): String = onEngine { nativeStartJob(code) }

    /** Returns the job state/result as JSON. */
    fun getJob(jobId: String): String = onEngine { nativeGetJob(jobId) }

    /** Drops a finished job. */
    fun clearJob(jobId: String) = onEngine { nativeClearJob(jobId) }

    private external fun nativeBootstrap(apiJsSource: String)
    private external fun nativeSetClient(nativeClientHandle: Long)
    private external fun nativeSetPluginCatalog(json: String)
    private external fun nativeRunScript(code: String): String
    private external fun nativeStartJob(code: String): String
    private external fun nativeGetJob(jobId: String): String
    private external fun nativeClearJob(jobId: String)
}
