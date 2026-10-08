package org.androidaudioplugin.aaphostsample

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.os.Handler
import android.os.Looper
import android.os.SystemClock
import android.util.Log
import kotlinx.coroutines.runBlocking
import org.androidaudioplugin.PortInformation
import org.androidaudioplugin.hosting.AudioPluginClientBase
import org.androidaudioplugin.hosting.AudioPluginHostHelper
import org.androidaudioplugin.hosting.InstanceState
import org.json.JSONArray
import org.json.JSONObject
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.util.concurrent.CountDownLatch
import java.util.concurrent.Executors
import java.util.concurrent.TimeUnit
import java.util.concurrent.atomic.AtomicBoolean
import java.util.concurrent.atomic.AtomicInteger
import java.util.concurrent.atomic.AtomicLong
import java.util.concurrent.atomic.AtomicReference
import java.util.concurrent.locks.LockSupport
import kotlin.math.sqrt

/** Debug-only numerical output probe; no hardware playback, tracing or per-block logs. */
class AapRealtimeProbeReceiver : BroadcastReceiver() {
    override fun onReceive(context: Context, intent: Intent) {
        val pending = goAsync()
        executor.execute {
            val result = try {
                if (intent.getStringExtra("phase") == "name_latency")
                    nameLatency(context.applicationContext, intent.getBooleanExtra("active", false))
                else if (intent.getStringExtra("phase") == "names") names(context.applicationContext)
                else run(context.applicationContext, intent.getStringExtra("phase") ?: "poll",
                    intent.getIntExtra("pollers", 8), intent.getIntExtra("blocks", 1000))
            } catch (t: Throwable) {
                Log.e(TAG, "probe failed", t)
                JSONObject().put("error", "${t.javaClass.name}: ${t.message}")
            }
            Log.i(TAG, "probe result: $result")
            pending.resultData = result.toString()
            pending.finish()
        }
    }

    private fun nameLatency(context: Context, active: Boolean): JSONObject {
        val packageName = "org.androidaudioplugin.ports.juce.byod"
        val plugin = AudioPluginHostHelper.queryAudioPluginServices(context, packageName)
            .flatMap { it.plugins }.first { it.pluginId == "juceaap:44717530" }
        val client = AudioPluginClientBase(context)
        runBlocking { client.connectToPluginService(packageName) }
        val instance = client.instantiateNativePlugin(plugin)
        val stop = AtomicBoolean(false)
        val failure = AtomicReference<Throwable?>(null)
        val requested = AtomicLong(0)
        val published = AtomicLong(0)
        val mainDelivered = AtomicLong(0)
        val main = Handler(Looper.getMainLooper())
        val changed = CountDownLatch(1)
        val delivered = CountDownLatch(1)
        val callbacks = AtomicInteger()
        var processor: Thread? = null
        try {
            instance.setParameterLayoutChangedListener {
                callbacks.incrementAndGet()
                if (requested.get() != 0L && instance.getParameterCount() > 9 &&
                    instance.getParameter(9).name == "Muff Drive: On/Off" &&
                    published.compareAndSet(0, SystemClock.elapsedRealtimeNanos())) {
                    changed.countDown()
                    main.post {
                        mainDelivered.set(SystemClock.elapsedRealtimeNanos())
                        delivered.countDown()
                    }
                }
            }
            instance.prepare(FRAMES, RATE, PORT_BYTES)
            if (active) {
                instance.activate()
                processor = Thread {
                    try {
                        val period = FRAMES * 1_000_000_000L / RATE
                        var next = System.nanoTime()
                        while (!stop.get()) {
                            val wait = next - System.nanoTime()
                            if (wait > 0) LockSupport.parkNanos(wait)
                            if (stop.get()) break
                            instance.process(FRAMES, 1_000_000_000L)
                            next = maxOf(next + period, System.nanoTime())
                        }
                    } catch (t: Throwable) { failure.set(t) }
                }.apply { name = "AAP.NamesProcess"; start() }
            }
            Thread.sleep(2000)
            val before = (0 until instance.getParameterCount()).map { instance.getParameter(it).name }
            requested.set(SystemClock.elapsedRealtimeNanos())
            instance.setCurrentPresetIndex(9)
            val returned = SystemClock.elapsedRealtimeNanos()
            val success = changed.await(20, TimeUnit.SECONDS)
            if (success) check(delivered.await(1, TimeUnit.SECONDS)) { "main callback did not run" }
            failure.get()?.let { throw it }
            val after = (0 until instance.getParameterCount()).map { instance.getParameter(it).name }
            fun millis(time: Long) = if (time == 0L) -1.0 else (time - requested.get()) / 1_000_000.0
            return JSONObject().put("phase", "name_latency").put("active", active)
                .put("updated", success).put("presetReturnMs", millis(returned))
                .put("publicationMs", millis(published.get())).put("mainDeliveryMs", millis(mainDelivered.get()))
                .put("callbacks", callbacks.get())
                .put("changedNames", before.zip(after).count { it.first != it.second })
                .put("name9", after.getOrNull(9))
        } finally {
            stop.set(true)
            processor?.join(3000)
            check(processor?.isAlive != true) { "processing did not stop" }
            instance.setParameterLayoutChangedListener(null)
            if (instance.state == InstanceState.ACTIVE) instance.deactivate()
            instance.destroy()
            client.disconnectPluginService(packageName)
            client.dispose()
        }
    }

    private fun names(context: Context): JSONObject {
        val packageName = "org.androidaudioplugin.ports.juce.byod"
        val plugin = AudioPluginHostHelper.queryAudioPluginServices(context, packageName)
            .flatMap { it.plugins }.first { it.pluginId == "juceaap:44717530" }
        val client = AudioPluginClientBase(context)
        runBlocking { client.connectToPluginService(packageName) }
        val instance = client.instantiateNativePlugin(plugin)
        val changes = AtomicInteger()
        try {
            instance.setParameterLayoutChangedListener { changes.incrementAndGet() }
            instance.prepare(FRAMES, RATE, PORT_BYTES)
            Thread.sleep(2000)
            fun snapshot() = (0 until instance.getParameterCount()).map { instance.getParameter(it).name }
            val before = snapshot()
            val previousChanges = changes.get()
            instance.setCurrentPresetIndex(9)
            Thread.sleep(6000)
            val after = snapshot()
            val changed = (0 until minOf(before.size, after.size)).filter { before[it] != after[it] }
            return JSONObject().put("phase", "names").put("preset", instance.getPresetName(9))
                .put("layoutChanges", changes.get() - previousChanges).put("changedNames", changed.size)
                .put("before", JSONArray(before.take(26))).put("after", JSONArray(after.take(26)))
        } finally {
            instance.setParameterLayoutChangedListener(null)
            instance.destroy()
            client.disconnectPluginService(packageName)
            client.dispose()
        }
    }

    private fun run(context: Context, phase: String, pollerCount: Int, blocks: Int): JSONObject {
        require(phase in listOf("poll", "note_off", "note_on", "overflow"))
        require(pollerCount in 0..8 && blocks in 64..4000)
        val plugin = AudioPluginHostHelper.queryAudioPluginServices(context, PACKAGE)
            .flatMap { it.plugins }.first { it.pluginId == PLUGIN }
        val client = AudioPluginClientBase(context)
        runBlocking { client.connectToPluginService(PACKAGE) }
        val instance = client.instantiateNativePlugin(plugin)
        val stop = AtomicBoolean(false)
        val stopPolling = AtomicBoolean(false)
        val controlPending = AtomicBoolean(false)
        val warm = CountDownLatch(1)
        val injected = CountDownLatch(1)
        val failure = AtomicReference<Throwable?>(null)
        val requests = AtomicInteger()
        val wrong = AtomicInteger()
        var processor: Thread? = null
        var pollers = emptyList<Thread>()
        var control: Thread? = null
        var measured = 0
        var silent = 0
        var nonFinite = 0
        var maxProcessNs = 0L
        var minimumRms = Double.POSITIVE_INFINITY
        var submitted = false
        var saturatedBlocks = 0
        try {
            instance.prepare(FRAMES, RATE, PORT_BYTES)
            val outputs = instance.getAudioPortIndices(PortInformation.PORT_DIRECTION_OUTPUT)
            val midi = instance.getMainEventPortIndex(PortInformation.PORT_DIRECTION_INPUT)
            check(midi >= 0) { "The plugin has no main event input bus" }
            val audio = outputs.map { ByteBuffer.allocateDirect(FRAMES * 4).order(ByteOrder.nativeOrder()) }
            val noteOn = ByteBuffer.allocateDirect(4).order(ByteOrder.nativeOrder()).putInt(0, 0x20903C7F)
            val event = ByteBuffer.allocateDirect(4).order(ByteOrder.nativeOrder()).putInt(0,
                if (phase == "note_on") 0x2191407F else 0x20803C00)
            val saturated = ByteBuffer.allocateDirect(PORT_BYTES).order(ByteOrder.nativeOrder())
            saturated.putInt(4, PORT_BYTES - HEADER_BYTES) // complete utility NOPs fill payload
            val expected = instance.getPresetCount()
            instance.activate()
            instance.addEventUmpInput(noteOn, 4)
            processor = Thread {
                try {
                    var block = 0
                    var next = System.nanoTime()
                    val period = FRAMES * 1_000_000_000L / RATE
                    while (!stop.get()) {
                        var wait = next - System.nanoTime()
                        while (wait > 0 && !stop.get()) { LockSupport.parkNanos(wait); wait = next - System.nanoTime() }
                        if (stop.get()) break
                        if (phase == "overflow" && submitted && controlPending.get()) {
                            instance.setPortBuffer(midi, saturated, PORT_BYTES)
                            saturatedBlocks++
                        }
                        val before = System.nanoTime()
                        instance.process(FRAMES, 1_000_000_000L)
                        val after = System.nanoTime()
                        maxProcessNs = maxOf(maxProcessNs, after - before)
                        next = maxOf(next + period, after)
                        var energy = 0.0
                        var finite = true
                        outputs.forEachIndexed { index, port ->
                            instance.getPortBuffer(port, audio[index], FRAMES * 4)
                            repeat(FRAMES) { frame ->
                                val sample = audio[index].getFloat(frame * 4).toDouble()
                                if (!sample.isFinite()) finite = false else energy += sample * sample
                            }
                        }
                        val rms = sqrt(energy / (FRAMES * outputs.size))
                        if (block >= WARMUP) {
                            measured++
                            if (rms == 0.0) silent++
                            if (!finite) nonFinite++
                            minimumRms = minOf(minimumRms, rms)
                        }
                        if (block == WARMUP) {
                            check(rms > 1e-5) { "sustained-note warmup is silent" }
                            warm.countDown()
                        }
                        if (phase != "poll" && controlPending.get() && !submitted && rms == 0.0) {
                            if (phase != "overflow") instance.addEventUmpInput(event, 4)
                            submitted = true
                            injected.countDown()
                        }
                        if (++block >= blocks && phase == "poll") break
                    }
                } catch (t: Throwable) { failure.set(t); warm.countDown(); injected.countDown() }
            }.apply { name = "AAP.ProbeProcess"; start() }
            check(warm.await(5, TimeUnit.SECONDS)) { "processing did not warm up" }
            failure.get()?.let { throw it }
            if (phase == "poll") {
                pollers = (0 until pollerCount).map {
                    Thread {
                        try {
                            while (!stopPolling.get()) {
                                if (instance.getPresetCount() != expected) wrong.incrementAndGet()
                                requests.incrementAndGet()
                            }
                        } catch (t: Throwable) { failure.set(t) }
                    }.apply { name = "AAP.ProbePoll.$it"; start() }
                }
                // Stop requests while processing still delivers any final replies.
                Thread.sleep((blocks - WARMUP - 8) * FRAMES * 1000L / RATE)
                stopPolling.set(true)
                pollers.forEach { it.join(3000) }
                check(pollers.none { it.isAlive }) { "poller did not finish" }
            } else {
                control = Thread {
                    controlPending.set(true)
                    try { instance.getStateSize() } catch (t: Throwable) { failure.set(t) }
                    finally { controlPending.set(false) }
                }.apply { name = "AAP.ProbeControl"; start() }
                check(injected.await(3, TimeUnit.SECONDS)) { "no suspended block observed; configure the sample control delay" }
                control.join(3000)
                check(!control.isAlive) { "control did not finish" }
                Thread.sleep(200)
            }
            stop.set(true)
            processor.join(3000)
            check(!processor.isAlive) { "processing did not finish" }
            failure.get()?.let { throw it }
            instance.deactivate()
            val state = ByteArray(instance.getStateSize())
            instance.getState(state)
            check(state.size >= 43) { "unexpected sample state size ${state.size}" }
            val notes = (40..42).map { state[it].toInt() }
            return JSONObject().put("phase", phase).put("pollers", pollerCount)
                .put("measuredBlocks", measured).put("silentBlocks", silent)
                .put("nonFiniteBlocks", nonFinite).put("minRms", minimumRms)
                .put("maxProcessUs", maxProcessNs / 1000)
                .put("countRequests", requests.get()).put("wrongCounts", wrong.get())
                .put("injected", submitted).put("saturatedBlocks", saturatedBlocks)
                .put("noteStates", JSONArray(notes))
        } finally {
            stopPolling.set(true)
            control?.join(3000)
            stop.set(true)
            processor?.join(3000)
            pollers.forEach { it.join(3000) }
            if (instance.state != InstanceState.DESTROYED) instance.destroy()
            client.disconnectPluginService(PACKAGE)
            client.dispose()
        }
    }
    companion object {
        private const val PACKAGE = "org.androidaudioplugin.aapinstrumentsample"
        private const val PLUGIN = "urn:org.androidaudioplugin/samples/aappluginsample/InstrumentSample"
        private const val FRAMES = 256
        private const val RATE = 48_000
        private const val PORT_BYTES = 65_536
        private const val HEADER_BYTES = 32
        private const val WARMUP = 32
        private const val TAG = "AAPRealtimeProbe"
        private val executor = Executors.newSingleThreadExecutor()
    }
}
