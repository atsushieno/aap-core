package org.androidaudioplugin.js

import android.content.Context
import android.util.Log

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
        nativeBootstrap(source)
        bootstrapped = true
        Log.i(LOG_TAG, "AapAutomationRuntime bootstrapped")
    }

    /**
     * Hands the connected native client pointer (`aap::PluginClient*`) to the runtime.
     * Obtain it from `NativePluginClient.native`. Pass 0 to detach.
     */
    fun attachNativeClient(nativeClientHandle: Long) = nativeSetClient(nativeClientHandle)

    /** Optional: feed the JVM-side plugin discovery result as JSON for `aap.discovery.getPlugins()`. */
    fun setPluginCatalog(json: String) = nativeSetPluginCatalog(json)

    /** Runs a script synchronously, returning its result as JSON (or an `ERROR: ...` string). */
    fun runScript(code: String): String = nativeRunScript(code)

    /** Starts a script as an async job; returns the job id to poll with [getJob]. */
    fun startJob(code: String): String = nativeStartJob(code)

    /** Returns the job state/result as JSON. */
    fun getJob(jobId: String): String = nativeGetJob(jobId)

    /** Drops a finished job. */
    fun clearJob(jobId: String) = nativeClearJob(jobId)

    private external fun nativeBootstrap(apiJsSource: String)
    private external fun nativeSetClient(nativeClientHandle: Long)
    private external fun nativeSetPluginCatalog(json: String)
    private external fun nativeRunScript(code: String): String
    private external fun nativeStartJob(code: String): String
    private external fun nativeGetJob(jobId: String): String
    private external fun nativeClearJob(jobId: String)
}
