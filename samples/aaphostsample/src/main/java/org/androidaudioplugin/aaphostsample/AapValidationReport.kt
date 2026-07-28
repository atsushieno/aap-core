package org.androidaudioplugin.aaphostsample

import android.os.SystemClock
import org.json.JSONArray
import org.json.JSONObject

/** Stable inputs shared by the adb receiver and any future validator UI or instrumentation. */
data class AapValidationRequest(
    val packageName: String,
    val sampleRate: Int,
    val frameCount: Int,
    val repeatCount: Int
)

/**
 * Transport-neutral validation result. JSON is intentionally produced here rather than in the
 * broadcast receiver, so a future UI or instrumentation runner uses exactly the same schema.
 */
class AapValidationReport(private val request: AapValidationRequest) {
    val findings = JSONArray()
    val plugins = JSONArray()
    private val startedAtMillis = SystemClock.elapsedRealtime()

    fun pass(pluginId: String?, id: String, title: String, observed: String) =
        finding(pluginId, "PASS", id, title, observed, null, null)

    fun fail(pluginId: String?, id: String, title: String, observed: String, consequence: String, fix: String) =
        finding(pluginId, "FAIL", id, title, observed, consequence, fix)

    fun skip(pluginId: String?, id: String, title: String, observed: String) =
        finding(pluginId, "SKIP", id, title, observed, null, null)

    private fun finding(
        pluginId: String?, status: String, id: String, title: String, observed: String,
        consequence: String?, fix: String?
    ) {
        findings.put(JSONObject().apply {
            if (pluginId != null) put("pluginId", pluginId)
            put("status", status)
            put("id", id)
            put("title", title)
            put("observed", observed)
            if (consequence != null) put("consequence", consequence)
            if (fix != null) put("fix", fix)
        })
    }

    fun toJson(): JSONObject = JSONObject().apply {
        put("schemaVersion", 1)
        put("validatorVersion", "0.1")
        put("packageName", request.packageName)
        put("sampleRate", request.sampleRate)
        put("frameCount", request.frameCount)
        put("repeatCount", request.repeatCount)
        put("plugins", plugins)
        put("findings", findings)
        val passes = count("PASS")
        val failures = count("FAIL")
        put("summary", JSONObject().put("passCount", passes).put("failCount", failures).put("skipCount", count("SKIP")))
        put("durationMillis", SystemClock.elapsedRealtime() - startedAtMillis)
        put("success", failures == 0)
    }

    private fun count(status: String): Int =
        (0 until findings.length()).count { findings.getJSONObject(it).getString("status") == status }
}
