package org.androidaudioplugin.aaphostsample

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.util.Log
import android.os.SystemClock
import kotlinx.coroutines.runBlocking
import org.androidaudioplugin.PortInformation
import org.androidaudioplugin.hosting.AudioPluginClientBase
import org.androidaudioplugin.hosting.AudioPluginHostHelper
import org.androidaudioplugin.hosting.NativeRemotePluginInstance
import org.json.JSONObject
import org.json.JSONArray
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.util.concurrent.Executors
import java.util.concurrent.atomic.AtomicBoolean
import java.util.concurrent.atomic.AtomicInteger
import java.util.concurrent.locks.LockSupport
import kotlin.random.Random

/**
 * Debug-only, adb-driven stress test for parameter layout refresh (aap-core#130).
 * It keeps an instance processing on one thread while another switches presets, so that
 * plugin-triggered parameter rescans overlap with audio processing, and validates every
 * published layout.
 *
 * adb shell am broadcast -a org.androidaudioplugin.aaphostsample.STRESS_PARAMETERS \
 *   -n org.androidaudioplugin.aaphostsample/.AapParameterStressReceiver --es package <pkg> --ei rounds 4
 */
class AapParameterStressReceiver : BroadcastReceiver() {
    override fun onReceive(context: Context, intent: Intent) {
        // Keep a run under the background broadcast timeout (~60s); goAsync() also keeps the process unfrozen.
        val pending = goAsync()
        executor.execute {
            val result = try {
                run(context.applicationContext,
                    intent.getStringExtra(EXTRA_PACKAGE) ?: throw IllegalArgumentException("Missing $EXTRA_PACKAGE"),
                    intent.getStringExtra(EXTRA_PLUGIN_ID),
                    intent.getIntExtra(EXTRA_ROUNDS, 4),
                    intent.getIntExtra(EXTRA_MAX_DELAY_MS, 50),
                    intent.getIntExtra(EXTRA_POLLERS, 1),
                    intent.getLongExtra(EXTRA_PROCESS_PERIOD_US, 0),
                    intent.getIntExtra(EXTRA_SETTLE_MS, SETTLE_MS.toInt()))
            } catch (t: Throwable) {
                Log.e(TAG, "stress failed", t)
                JSONObject().put("error", "${t.javaClass.name}: ${t.message.orEmpty()}")
            }
            Log.i(TAG, "stress result: $result")
            pending.resultData = result.toString()
            pending.finish()
        }
    }

    private fun run(context: Context, packageName: String, pluginId: String?, rounds: Int, maxDelayMs: Int,
                    pollerCount: Int, processPeriodUs: Long, settleMs: Int): JSONObject {
        require(processPeriodUs in 0..1_000_000 && settleMs in 0..20_000)
        val plugin = AudioPluginHostHelper.queryAudioPluginServices(context, packageName)
            .flatMap { it.plugins }
            .firstOrNull { pluginId == null || it.pluginId == pluginId }
            ?: throw IllegalArgumentException("No plugin found in $packageName")
        val start = SystemClock.elapsedRealtime()
        fun elapsedMs() = SystemClock.elapsedRealtime() - start
        fun step(name: String) = Log.i(TAG, "$name at ${elapsedMs()}ms")
        val client = AudioPluginClientBase(context)
        runBlocking { client.connectToPluginService(plugin.packageName) }
        step("connected")
        val instance = client.instantiateNativePlugin(plugin)
        step("instantiated")
        val layoutChanges = AtomicInteger()
        val layoutChangeTimesMs = mutableListOf<Long>()
        val invalidLayouts = mutableListOf<String>()
        val processCount = AtomicInteger()
        val stop = AtomicBoolean(false)
        var maxProcessNs = 0L // processor only, read after join
        var processOverruns = 0
        try {
            instance.prepare(FRAME_COUNT, SAMPLE_RATE, CONTROL_BYTES_PER_BLOCK)
            step("prepared")
            writeSilentAudioInputs(instance)
            instance.setParameterLayoutChangedListener {
                layoutChanges.incrementAndGet()
                synchronized(layoutChangeTimesMs) { layoutChangeTimesMs.add(elapsedMs()) }
                step("layout published")
                validateLayout(instance)?.let { synchronized(invalidLayouts) { invalidLayouts.add(it) } }
            }
            instance.activate()
            step("activated")
            val processor = Thread {
                val periodNs = processPeriodUs * 1000
                var nextBlock = System.nanoTime()
                while (!stop.get()) {
                    if (periodNs > 0) {
                        var remaining = nextBlock - System.nanoTime()
                        while (remaining > 0 && !stop.get()) {
                            LockSupport.parkNanos(remaining)
                            remaining = nextBlock - System.nanoTime()
                        }
                        if (stop.get()) break
                    }
                    val before = if (periodNs > 0) System.nanoTime() else 0L
                    instance.process(FRAME_COUNT, PROCESS_TIMEOUT_NANOSECONDS)
                    processCount.incrementAndGet()
                    if (periodNs > 0) {
                        val after = System.nanoTime()
                        val duration = after - before
                        maxProcessNs = maxOf(maxProcessNs, duration)
                        if (duration > periodNs) processOverruns++
                        // Do not turn a late block into a burst of catch-up calls.
                        nextBlock = maxOf(nextBlock + periodNs, after)
                    }
                }
            }.apply { name = "AAP.StressProcess"; start() }

            val lastProgress = java.util.concurrent.atomic.AtomicLong(System.currentTimeMillis())
            val watchdog = Thread {
                while (!stop.get()) {
                    Thread.sleep(1000)
                    if (System.currentTimeMillis() - lastProgress.get() > STALL_MS) {
                        for ((t, st) in Thread.getAllStackTraces())
                            Log.w(TAG, "stall: ${t.name}\n  " + st.joinToString("\n  "))
                        lastProgress.set(System.currentTimeMillis())
                    }
                }
            }.apply { isDaemon = true; start() }

            val presetCount = instance.getPresetCount()
            // get_preset_count is RT-safe, so active requests go over SysEx8. Processing
            // drives replies; the extension worker completes them alongside Binder control work.
            val sysex8Requests = AtomicInteger()
            val badPresetCounts = AtomicInteger()
            val stopPolling = AtomicBoolean(false)
            val pollers = (0 until pollerCount).map {
                Thread {
                    while (!stopPolling.get()) {
                        val count = instance.getPresetCount()
                        if (count != presetCount && badPresetCounts.incrementAndGet() <= 5)
                            Log.w(TAG, "unexpected preset count: $count")
                        sysex8Requests.incrementAndGet()
                    }
                }.apply { name = "AAP.StressSysEx8.$it"; start() }
            }
            step("got $presetCount presets")
            val random = Random(0)
            repeat(rounds) { round ->
                Log.i(TAG, "round $round: ${processCount.get()} process calls, ${layoutChanges.get()} layout changes")
                for (preset in 0 until presetCount) {
                    instance.setCurrentPresetIndex(preset)
                    lastProgress.set(System.currentTimeMillis())
                    Thread.sleep(random.nextLong(0, maxDelayMs + 1L))
                }
            }
            val switchesDoneMs = elapsedMs()
            step("preset switches finished")
            Thread.sleep(settleMs.toLong())
            // SysEx8 replies arrive only while processing, so stop polling first.
            stopPolling.set(true)
            pollers.forEach { it.join() }
            stop.set(true)
            processor.join()
            instance.deactivate()

            return JSONObject()
                .put("plugin", plugin.pluginId)
                .put("presets", presetCount)
                .put("rounds", rounds)
                .put("processCalls", processCount.get())
                .put("processPeriodUs", processPeriodUs)
                .put("maxProcessUs", maxProcessNs / 1000)
                .put("processOverruns", processOverruns)
                .put("sysex8Requests", sysex8Requests.get())
                .put("badPresetCounts", badPresetCounts.get())
                .put("layoutChanges", layoutChanges.get())
                .put("layoutChangeTimesMs", synchronized(layoutChangeTimesMs) { JSONArray(layoutChangeTimesMs) })
                .put("switchesDoneMs", switchesDoneMs)
                .put("elapsedMs", elapsedMs())
                .put("parameters", instance.getParameterCount())
                .put("invalidLayouts", synchronized(invalidLayouts) { invalidLayouts.toList() })
                .put("state", instance.state.name)
        } finally {
            stop.set(true)
            instance.setParameterLayoutChangedListener(null)
            instance.destroy()
            step("destroyed")
            client.disconnectPluginService(plugin.packageName)
            client.dispose()
            step("disposed")
        }
    }

    // Returns a description of the first problem, or null if the layout looks sane.
    private fun validateLayout(instance: NativeRemotePluginInstance): String? {
        val count = instance.getParameterCount()
        if (count !in 0..MAX_ITEMS)
            return "parameter count $count"
        for (i in 0 until count) {
            val p = instance.getParameter(i)
            if (p.id !in 0 until MAX_ITEMS)
                return "parameter #$i has id ${p.id}"
            if (p.enumerations.size > MAX_ITEMS)
                return "parameter ${p.id} has ${p.enumerations.size} enumerations"
        }
        return null
    }

    private fun writeSilentAudioInputs(instance: NativeRemotePluginInstance) {
        val byteCount = FRAME_COUNT * Float.SIZE_BYTES
        for (index in 0 until instance.getPortCount()) {
            val port = instance.getPort(index)
            if (port.content != PortInformation.PORT_CONTENT_TYPE_AUDIO ||
                port.direction != PortInformation.PORT_DIRECTION_INPUT)
                continue
            instance.setPortBuffer(index, ByteBuffer.allocateDirect(byteCount).order(ByteOrder.nativeOrder()), byteCount)
        }
    }

    companion object {
        const val EXTRA_PACKAGE = "package"
        const val EXTRA_PLUGIN_ID = "plugin_id"
        const val EXTRA_ROUNDS = "rounds"
        const val EXTRA_MAX_DELAY_MS = "max_delay_ms"
        const val EXTRA_POLLERS = "pollers"
        const val EXTRA_PROCESS_PERIOD_US = "process_period_us"
        const val EXTRA_SETTLE_MS = "settle_ms"
        private const val SAMPLE_RATE = 48_000
        private const val FRAME_COUNT = 256
        private const val CONTROL_BYTES_PER_BLOCK = 0x10000
        private const val PROCESS_TIMEOUT_NANOSECONDS = 1_000_000_000L
        private const val SETTLE_MS = 3000L
        private const val STALL_MS = 10_000L
        private const val MAX_ITEMS = 16384
        private const val TAG = "AAPParamStress"
        private val executor = Executors.newSingleThreadExecutor()
    }
}
