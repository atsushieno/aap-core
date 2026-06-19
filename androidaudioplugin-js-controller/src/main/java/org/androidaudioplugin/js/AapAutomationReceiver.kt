package org.androidaudioplugin.js

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.util.Base64
import android.util.Log
import java.util.concurrent.Executors

/**
 * adb-driven entry point for the embedded JS automation runtime — same protocol shape as
 * uapmd-app's `AutomationReceiver`.
 *
 * The host fires a broadcast and reads the JS result from `setResultData`:
 *
 * ```sh
 * CODE='JSON.stringify(aap.ping())'
 * adb shell am broadcast \
 *   -a org.androidaudioplugin.js.RUN_JS \
 *   -n <your.app.package>/org.androidaudioplugin.js.AapAutomationReceiver \
 *   --es code "$CODE"
 * ```
 *
 * Long scripts (that may stall on a binder connection) use `RUN_JS_ASYNC`, which returns a job id;
 * poll it with `GET_JS_JOB` and drop it with `CLEAR_JS_JOB`.
 *
 * All scripts run on a single executor thread, matching the runtime's single-threaded contract.
 *
 * SECURITY: this receiver is exported and runs arbitrary JavaScript against the host's plugin
 * client. It is intended for debug/testing builds. Apps that ship it in release should gate it
 * (e.g. remove via a release manifest, or guard on `BuildConfig.DEBUG`).
 */
class AapAutomationReceiver : BroadcastReceiver() {
    override fun onReceive(context: Context, intent: Intent) {
        val pending = goAsync()
        executor.execute {
            try {
                when (intent.action) {
                    ACTION_GET_JS_JOB -> {
                        val jobId = intent.getStringExtra(EXTRA_JOB_ID)
                        if (jobId.isNullOrBlank())
                            return@execute fail(pending, 5, "Missing $EXTRA_JOB_ID")
                        succeed(pending, AapAutomationRuntime.getJob(jobId))
                    }
                    ACTION_CLEAR_JS_JOB -> {
                        val jobId = intent.getStringExtra(EXTRA_JOB_ID)
                        if (jobId.isNullOrBlank())
                            return@execute fail(pending, 5, "Missing $EXTRA_JOB_ID")
                        AapAutomationRuntime.clearJob(jobId)
                        succeed(pending, """{"jobId":"$jobId","cleared":true}""")
                    }
                    ACTION_RUN_JS, ACTION_RUN_JS_ASYNC -> {
                        // Make sure the facade is loaded even if the app forgot to bootstrap explicitly.
                        AapAutomationRuntime.bootstrap(context)
                        val code = decodeCode(intent)
                            ?: return@execute fail(pending, 3, "Missing $EXTRA_CODE or $EXTRA_CODE_BASE64")
                        if (intent.action == ACTION_RUN_JS) {
                            succeed(pending, AapAutomationRuntime.runScript(code))
                        } else {
                            succeed(pending, AapAutomationRuntime.startJob(code))
                        }
                    }
                    else -> fail(pending, 4, "Unsupported action: ${intent.action}")
                }
            } catch (t: Throwable) {
                Log.e(TAG, "automation failed", t)
                fail(pending, 1, "${t.javaClass.name}: ${t.message ?: ""}".trim())
            } finally {
                pending.finish()
            }
        }
    }

    private fun decodeCode(intent: Intent): String? = when {
        intent.hasExtra(EXTRA_CODE) -> intent.getStringExtra(EXTRA_CODE)
        intent.hasExtra(EXTRA_CODE_BASE64) -> intent.getStringExtra(EXTRA_CODE_BASE64)?.let {
            if (it.isEmpty()) null else String(Base64.decode(it, Base64.DEFAULT), Charsets.UTF_8)
        }
        else -> null
    }?.takeIf { it.isNotBlank() }

    private fun succeed(pending: PendingResult, data: String) {
        Log.i(TAG, "result: $data")
        pending.setResultCode(0)
        pending.setResultData(data)
    }

    private fun fail(pending: PendingResult, code: Int, message: String) {
        Log.e(TAG, message)
        pending.setResultCode(code)
        pending.setResultData(message)
    }

    companion object {
        const val ACTION_RUN_JS = "org.androidaudioplugin.js.RUN_JS"
        const val ACTION_RUN_JS_ASYNC = "org.androidaudioplugin.js.RUN_JS_ASYNC"
        const val ACTION_GET_JS_JOB = "org.androidaudioplugin.js.GET_JS_JOB"
        const val ACTION_CLEAR_JS_JOB = "org.androidaudioplugin.js.CLEAR_JS_JOB"
        const val EXTRA_CODE = "code"
        const val EXTRA_CODE_BASE64 = "code_base64"
        const val EXTRA_JOB_ID = "job_id"
        private const val TAG = "aap-js-controller"
        private val executor = Executors.newSingleThreadExecutor()
    }
}
