package org.androidaudioplugin.aaphostsample

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.util.Log
import kotlinx.coroutines.runBlocking
import org.androidaudioplugin.PortInformation
import org.androidaudioplugin.hosting.AudioPluginClientBase
import org.androidaudioplugin.hosting.AudioPluginHostHelper
import org.androidaudioplugin.hosting.InstanceState
import org.androidaudioplugin.hosting.NativeRemotePluginInstance
import org.androidaudioplugin.hosting.UmpHelper
import org.json.JSONObject
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.util.concurrent.Executors

/**
 * Debug-only, adb-driven reproduction of session reloads (aap-core#226, #227): an instance is
 * repeatedly destroyed and re-created, restored with setState() of a saved state, then processed.
 *
 * adb shell am broadcast -a org.androidaudioplugin.aaphostsample.STATE_RELOAD \
 *   -n org.androidaudioplugin.aaphostsample.validator/org.androidaudioplugin.aaphostsample.AapStateReloadReceiver \
 *   --es package <pkg> --ei reloads 10
 */
class AapStateReloadReceiver : BroadcastReceiver() {
    override fun onReceive(context: Context, intent: Intent) {
        // Keep a run under the background broadcast timeout (~60s); goAsync() also keeps the process unfrozen.
        val pending = goAsync()
        executor.execute {
            val result = try {
                run(context.applicationContext,
                    intent.getStringExtra(EXTRA_PACKAGE) ?: throw IllegalArgumentException("Missing $EXTRA_PACKAGE"),
                    intent.getStringExtra(EXTRA_PLUGIN_ID),
                    intent.getIntExtra(EXTRA_RELOADS, 10),
                    intent.getIntExtra(EXTRA_PRESET, 1),
                    intent.getIntExtra(EXTRA_BLOCKS, 8),
                    intent.getBooleanExtra(EXTRA_PACED, false),
                    intent.getBooleanExtra(EXTRA_RANDOMIZE, false),
                    intent.getLongExtra(EXTRA_SETTLE_MS, 0))
            } catch (t: Throwable) {
                Log.e(TAG, "reload failed", t)
                JSONObject().put("error", "${t.javaClass.name}: ${t.message.orEmpty()}")
            }
            Log.i(TAG, "reload result: $result")
            pending.resultData = result.toString()
            pending.finish()
        }
    }

    private fun run(context: Context, packageName: String, pluginId: String?, reloads: Int, preset: Int, blocks: Int, paced: Boolean, randomize: Boolean, settleMs: Long): JSONObject {
        val plugin = AudioPluginHostHelper.queryAudioPluginServices(context, packageName)
            .flatMap { it.plugins }
            .firstOrNull { pluginId == null || it.pluginId == pluginId }
            ?: throw IllegalArgumentException("No plugin found in $packageName")
        val client = AudioPluginClientBase(context)
        runBlocking { client.connectToPluginService(plugin.packageName) }
        var maxOutputMidiLength = 0
        var nonFiniteBlocks = 0
        var failedReloads = 0
        try {
            // Make a state that differs from the defaults, as a saved session would.
            // The plugin may not survive it either; then reload without state.
            val state = runCatching { withInstance(client, plugin) { instance ->
                if (preset in 0 until instance.getPresetCount())
                    instance.setCurrentPresetIndex(preset)
                instance.activate()
                // A saved session may differ from the defaults in (almost) every parameter.
                if (randomize)
                    randomizeParameters(instance)
                repeat(blocks) { instance.process(PROCESS_FRAMES, PROCESS_TIMEOUT_NANOSECONDS) }
                instance.deactivate()
                ByteArray(instance.getStateSize()).also { instance.getState(it) }
            } }.getOrElse {
                Log.w(TAG, "saving state failed: ${it.javaClass.simpleName}: ${it.message}")
                client.disconnectPluginService(plugin.packageName)
                Thread.sleep(RECONNECT_DELAY_MS)
                runBlocking { client.connectToPluginService(plugin.packageName) }
                ByteArray(0)
            }
            Log.i(TAG, "saved state: ${state.size} bytes")

            repeat(reloads) { reload ->
                try {
                withInstance(client, plugin) { instance ->
                    if (state.isNotEmpty())
                        instance.setState(state)
                    // a host restoring a session does not necessarily start processing right away
                    Thread.sleep(settleMs)
                    instance.activate()
                    repeat(blocks) {
                        // like an audio callback, so that the plugin's own threads progress between blocks
                        if (paced)
                            Thread.sleep(PROCESS_FRAMES * 1000L / SAMPLE_RATE)
                        instance.process(PROCESS_FRAMES, PROCESS_TIMEOUT_NANOSECONDS)
                        maxOutputMidiLength = maxOf(maxOutputMidiLength, outputMidiLength(instance))
                        if (hasNonFiniteOutput(instance))
                            nonFiniteBlocks++
                    }
                    instance.deactivate()
                    if (instance.state == InstanceState.ERROR)
                        throw IllegalStateException("instance entered ERROR state at reload $reload")
                }
                Log.i(TAG, "reload $reload done")
                } catch (t: Throwable) {
                    // e.g. the plugin process died; keep reloading like a host would.
                    failedReloads++
                    Log.w(TAG, "reload $reload failed: ${t.javaClass.simpleName}: ${t.message}")
                    client.disconnectPluginService(plugin.packageName)
                    Thread.sleep(RECONNECT_DELAY_MS)
                    runBlocking { client.connectToPluginService(plugin.packageName) }
                }
            }
            return JSONObject()
                .put("plugin", plugin.pluginId)
                .put("stateBytes", state.size)
                .put("reloads", reloads)
                .put("maxOutputMidiLength", maxOutputMidiLength)
                .put("nonFiniteBlocks", nonFiniteBlocks)
                .put("failedReloads", failedReloads)
        } finally {
            client.disconnectPluginService(plugin.packageName)
            client.dispose()
        }
    }

    private fun <T> withInstance(client: AudioPluginClientBase, plugin: org.androidaudioplugin.PluginInformation,
                                 action: (NativeRemotePluginInstance) -> T): T {
        val instance = client.instantiateNativePlugin(plugin)
        try {
            instance.prepare(PREPARE_FRAMES, SAMPLE_RATE, CONTROL_BYTES_PER_BLOCK)
            writeSilentAudioInputs(instance)
            return action(instance)
        } finally {
            if (instance.state != InstanceState.DESTROYED)
                instance.destroy()
        }
    }

    private fun randomizeParameters(instance: NativeRemotePluginInstance) {
        val words = (0 until instance.getParameterCount()).flatMap { i ->
            val p = instance.getParameter(i)
            // the extreme farther from the default, so that every parameter differs from it
            val plain = if (p.defaultValue - p.minimumValue < p.maximumValue - p.defaultValue) p.maximumValue else p.minimumValue
            UmpHelper.aapUmpSysex8ParameterPlain(p.id.toUInt(), p.minimumValue, p.maximumValue, plain).toList()
        }
        // The MIDI2 input port is small; send them over several blocks.
        for (chunk in words.chunked(MIDI_INPUT_WORDS_PER_BLOCK)) {
            val buffer = ByteBuffer.allocateDirect(chunk.size * Int.SIZE_BYTES).order(ByteOrder.nativeOrder())
            chunk.forEach { buffer.putInt(it) }
            instance.addEventUmpInput(buffer, chunk.size * Int.SIZE_BYTES)
            instance.process(PROCESS_FRAMES, PROCESS_TIMEOUT_NANOSECONDS)
        }
    }

    // The length the plugin wrote into its MIDI2 output header, which the host must not trust.
    private fun outputMidiLength(instance: NativeRemotePluginInstance): Int {
        var max = 0
        val index = instance.getMainEventPortIndex(PortInformation.PORT_DIRECTION_OUTPUT)
        if (index >= 0) {
            val buffer = ByteBuffer.allocateDirect(MIDI_HEADER_BYTES).order(ByteOrder.nativeOrder())
            instance.getPortBuffer(index, buffer, MIDI_HEADER_BYTES)
            max = maxOf(max, buffer.getInt(MIDI_HEADER_LENGTH_OFFSET))
        }
        return max
    }

    private fun hasNonFiniteOutput(instance: NativeRemotePluginInstance): Boolean {
        val byteCount = PROCESS_FRAMES * Float.SIZE_BYTES
        for (index in instance.getAudioPortIndices(PortInformation.PORT_DIRECTION_OUTPUT)) {
            val buffer = ByteBuffer.allocateDirect(byteCount).order(ByteOrder.nativeOrder())
            instance.getPortBuffer(index, buffer, byteCount)
            val samples = buffer.asFloatBuffer()
            for (i in 0 until PROCESS_FRAMES)
                if (!samples.get(i).isFinite())
                    return true
        }
        return false
    }

    private fun writeSilentAudioInputs(instance: NativeRemotePluginInstance) {
        val byteCount = PREPARE_FRAMES * Float.SIZE_BYTES
        for (index in instance.getAudioPortIndices(PortInformation.PORT_DIRECTION_INPUT)) {
            instance.setPortBuffer(index, ByteBuffer.allocateDirect(byteCount).order(ByteOrder.nativeOrder()), byteCount)
        }
    }

    companion object {
        const val EXTRA_PACKAGE = "package"
        const val EXTRA_PLUGIN_ID = "plugin_id"
        const val EXTRA_RELOADS = "reloads"
        const val EXTRA_PRESET = "preset"
        const val EXTRA_BLOCKS = "blocks"
        const val EXTRA_PACED = "paced"
        const val EXTRA_RANDOMIZE = "randomize"
        const val EXTRA_SETTLE_MS = "settle_ms"
        private const val SAMPLE_RATE = 48_000
        private const val PREPARE_FRAMES = 4096
        private const val PROCESS_FRAMES = 512
        private const val CONTROL_BYTES_PER_BLOCK = 0x10000
        private const val PROCESS_TIMEOUT_NANOSECONDS = 1_000_000_000L
        private const val MIDI_HEADER_BYTES = 32
        private const val MIDI_INPUT_WORDS_PER_BLOCK = 256
        private const val RECONNECT_DELAY_MS = 500L
        private const val MIDI_HEADER_LENGTH_OFFSET = 4 // AAPMidiBufferHeader::length
        private const val TAG = "AAPStateReload"
        private val executor = Executors.newSingleThreadExecutor()
    }
}
