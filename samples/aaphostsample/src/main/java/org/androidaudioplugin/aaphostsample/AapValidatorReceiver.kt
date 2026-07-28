package org.androidaudioplugin.aaphostsample

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.util.Log
import kotlinx.coroutines.runBlocking
import org.androidaudioplugin.ParameterInformation
import org.androidaudioplugin.PluginInformation
import org.androidaudioplugin.PortInformation
import org.androidaudioplugin.hosting.AudioPluginClientBase
import org.androidaudioplugin.hosting.AudioPluginExtensionsBom
import org.androidaudioplugin.hosting.AudioPluginHostHelper
import org.androidaudioplugin.hosting.InstanceState
import org.androidaudioplugin.hosting.NativeRemotePluginInstance
import org.androidaudioplugin.hosting.UmpHelper
import org.json.JSONObject
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.util.concurrent.Executors

/**
 * Debug-only, adb-driven validator entry point.
 *
 * Unlike the sample host UI, this receiver creates its client with the application context. That
 * keeps the plugin service bound for the duration of validation without requesting foreground
 * execution or notification permission.
 */
class AapValidatorReceiver : BroadcastReceiver() {
    override fun onReceive(context: Context, intent: Intent) {
        val pending = goAsync()
        executor.execute {
            try {
                Log.i(TAG, "received validation request")
                if (intent.action != ACTION_VALIDATE) {
                    fail(pending, 4, "Unsupported action: ${intent.action}")
                    return@execute
                }
                val packageName = intent.getStringExtra(EXTRA_PACKAGE)?.takeIf { it.isNotBlank() }
                if (packageName == null) {
                    fail(pending, 3, "Missing $EXTRA_PACKAGE")
                    return@execute
                }
                val request = AapValidationRequest(
                    packageName = packageName,
                    sampleRate = intent.getIntExtra(EXTRA_SAMPLE_RATE, DEFAULT_SAMPLE_RATE),
                    frameCount = intent.getIntExtra(EXTRA_FRAME_COUNT, DEFAULT_FRAME_COUNT),
                    repeatCount = intent.getIntExtra(EXTRA_REPEAT_COUNT, DEFAULT_REPEAT_COUNT)
                )
                succeed(pending, AapValidator(context.applicationContext).run(request).toJson().toString())
            } catch (t: Throwable) {
                Log.e(TAG, "validator failed", t)
                fail(pending, 1, "${t.javaClass.name}: ${t.message.orEmpty()}")
            } finally {
                pending.finish()
            }
        }
    }

    private fun succeed(pending: PendingResult, data: String) {
        Log.i(TAG, "validation result: $data")
        pending.setResultCode(0)
        pending.setResultData(data)
    }

    private fun fail(pending: PendingResult, code: Int, message: String) {
        Log.e(TAG, message)
        pending.setResultCode(code)
        pending.setResultData(JSONObject().put("error", message).toString())
    }

    companion object {
        const val ACTION_VALIDATE = "org.androidaudioplugin.aaphostsample.VALIDATE"
        const val EXTRA_PACKAGE = "package"
        const val EXTRA_SAMPLE_RATE = "sample_rate"
        const val EXTRA_FRAME_COUNT = "frame_count"
        const val EXTRA_REPEAT_COUNT = "repeat_count"
        private const val DEFAULT_SAMPLE_RATE = 48_000
        private const val DEFAULT_FRAME_COUNT = 256
        private const val DEFAULT_REPEAT_COUNT = 2
        private const val TAG = "AAPValidator"
        private val executor = Executors.newSingleThreadExecutor()
    }
}

private class AapValidator(private val context: Context) {
    fun run(request: AapValidationRequest): AapValidationReport {
        val report = AapValidationReport(request)
        if (request.sampleRate <= 0 || request.frameCount <= 0 || request.repeatCount <= 0) {
            report.fail(null, "AAPVAL-REQ-001", "Invalid execution settings",
                "sampleRate=${request.sampleRate}, frameCount=${request.frameCount}, repeatCount=${request.repeatCount}",
                "The validator cannot prepare an audio instance with these values.",
                "Use positive sample rate, frame count, and repeat count values.")
            return report
        }

        try {
            context.packageManager.getPackageInfo(request.packageName, 0)
        } catch (_: PackageManager.NameNotFoundException) {
            report.fail(null, "AAPVAL-APK-001", "Plugin package is not installed",
                "Package ${request.packageName} is not installed on this device.",
                "The validator cannot inspect or bind a package that Android has not installed.",
                "Install the APK, then pass its exact application ID (for example from `adb shell pm list packages`).")
            return report
        }

        val discovery = AudioPluginHostHelper.queryAudioPluginServicesWithDiagnostics(context, request.packageName)
        discovery.diagnostics.forEach { diagnostic ->
            report.fail(null, "AAPVAL-DISC-002", "AAP metadata could not be read", diagnostic,
                "Hosts may ignore the affected service or receive incomplete plugin metadata.",
                "Correct the referenced aap_metadata.xml. Parser messages include line and column when Android provides them.")
        }
        val services = discovery.services
        if (services.isEmpty()) {
            report.fail(null, "AAPVAL-DISC-001", "AAP service is not discoverable",
                "Package ${request.packageName} exposed no readable AAP services.",
                "An AAP host cannot discover or bind the plugin.",
                "Check the service intent action, exported declaration, package visibility, and AAP metadata resource.")
            return report
        }
        report.pass(null, "AAPVAL-DISC-001", "AAP service is discoverable",
            "Found ${services.size} service(s) in ${request.packageName}.")

        services.forEach { service ->
            service.plugins.forEach { plugin ->
                report.plugins.put(JSONObject().put("pluginId", plugin.pluginId ?: "<missing-id>")
                    .put("displayName", plugin.displayName))
                validateMetadata(plugin, report)
                validateLifecycle(plugin, request, report)
            }
        }
        return report
    }

    private fun validateMetadata(plugin: PluginInformation, report: AapValidationReport) {
        val pluginName = plugin.pluginId ?: "<missing-id>"
        if (plugin.pluginId.isNullOrBlank()) {
            report.fail(pluginName, "AAPVAL-META-001", "Plugin ID is missing", "Plugin $pluginName has no unique-id.",
                "Hosts cannot identify or instantiate the plugin.", "Set a non-empty unique-id in AAP metadata.")
        }
        if (plugin.sharedLibraryName.isNullOrBlank()) {
            report.fail(pluginName, "AAPVAL-META-002", "Native library is missing", "Plugin $pluginName has no library attribute.",
                "The service cannot load the plugin factory.", "Set the plugin library attribute to the packaged native library.")
        }
        val duplicatePortIndices = plugin.ports.groupBy { it.index }.filterValues { it.size > 1 }.keys
        if (duplicatePortIndices.isNotEmpty()) {
            report.fail(pluginName, "AAPVAL-META-010", "Port indices are duplicated", "Plugin $pluginName duplicates ${duplicatePortIndices.joinToString()}.",
                "Hosts cannot address ports unambiguously.", "Assign each declared port a unique index.")
        }
        plugin.ports.filter { it.minimumSizeInBytes < 0 }.forEach {
            report.fail(pluginName, "AAPVAL-META-011", "Port minimum size is negative", "Port ${it.index} has minimumSize=${it.minimumSizeInBytes}.",
                "Hosts cannot allocate a negative buffer size.", "Use a non-negative minimumSize value.")
        }
        plugin.ports.filter { it.direction !in 0..1 || it.content !in 0..3 }.forEach {
            report.fail(pluginName, "AAPVAL-META-012", "Port type is invalid",
                "Port ${it.index} has direction=${it.direction}, content=${it.content}.",
                "Hosts cannot determine how to configure this port.", "Use defined AAP port direction and content values.")
        }
        val duplicateParameterIds = plugin.parameters.groupBy { it.id }.filterValues { it.size > 1 }.keys
        if (duplicateParameterIds.isNotEmpty()) {
            report.fail(pluginName, "AAPVAL-META-021", "Parameter IDs are duplicated",
                "Plugin $pluginName duplicates ${duplicateParameterIds.joinToString()}.",
                "Hosts cannot address parameters unambiguously.", "Assign each parameter a unique ID.")
        }
        plugin.parameters.forEach { parameter -> validateParameter(pluginName, parameter, report) }
    }

    private fun validateParameter(pluginName: String, parameter: ParameterInformation, report: AapValidationReport) {
        val values = listOf(parameter.minimumValue, parameter.defaultValue, parameter.maximumValue)
        if (values.any { !it.isFinite() } || parameter.minimumValue > parameter.maximumValue ||
            parameter.defaultValue !in parameter.minimumValue..parameter.maximumValue) {
            report.fail(pluginName, "AAPVAL-META-020", "Parameter metadata is inconsistent",
                "Plugin $pluginName parameter ${parameter.id} has min=${parameter.minimumValue}, default=${parameter.defaultValue}, max=${parameter.maximumValue}.",
                "Hosts cannot present or safely automate this parameter.",
                "Use finite values with minimum <= default <= maximum.")
        }
        parameter.enumerations.forEach { enumeration ->
            if (!enumeration.value.isFinite() || enumeration.name.isBlank()) {
                report.fail(pluginName, "AAPVAL-META-022", "Parameter enumeration is invalid",
                    "Plugin $pluginName parameter ${parameter.id} has enumeration value=${enumeration.value}, name='${enumeration.name}'.",
                    "Hosts cannot present a stable enumeration choice.", "Use a finite value and non-empty enumeration name.")
            }
        }
    }

    private fun validateLifecycle(plugin: PluginInformation, request: AapValidationRequest, report: AapValidationReport) {
        val pluginId = plugin.pluginId
        if (pluginId.isNullOrBlank())
            return
        val client = AudioPluginClientBase(context)
        var instance: NativeRemotePluginInstance? = null
        try {
            runBlocking { client.connectToPluginService(plugin.packageName) }
            repeat(request.repeatCount) { iteration ->
                val current = client.instantiateNativePlugin(plugin)
                instance = current
                current.prepare(request.frameCount, request.sampleRate, DEFAULT_CONTROL_BYTES_PER_BLOCK)
                assertHealthy(current, "prepare")
                writeSilentAudioInputs(current, request.frameCount)
                current.activate()
                assertHealthy(current, "activate")
                queueMidiNoteProbe(pluginId, current, report)
                queueParameterProbe(plugin, current, report)
                current.process(request.frameCount, PROCESS_TIMEOUT_NANOSECONDS)
                assertHealthy(current, "process")
                validateFiniteAudioOutputs(pluginId, current, request.frameCount, report)
                validateParameterExtension(plugin, current, report)
                validateStateExtension(plugin, current, report)
                validatePresetExtension(plugin, current, report)
                current.deactivate()
                assertHealthy(current, "deactivate")
                current.destroy()
                assertHealthy(current, "destroy")
                instance = null
                report.pass(pluginId, "AAPVAL-LIFE-001", "Plugin lifecycle completed", "Plugin $pluginId completed lifecycle run ${iteration + 1}/${request.repeatCount}.")
            }
        } catch (t: Throwable) {
            report.fail(pluginId, "AAPVAL-LIFE-001", "Plugin lifecycle failed", "Plugin $pluginId: ${t.javaClass.simpleName}: ${t.message.orEmpty()}",
                "A normal AAP host cannot safely use this plugin.", "Check the service log and factory/lifecycle implementation.")
        } finally {
            try {
                instance?.takeIf { it.state != InstanceState.DESTROYED }?.destroy()
            } catch (_: Throwable) {
            }
            client.disconnectPluginService(plugin.packageName)
            client.dispose()
        }
    }

    private fun assertHealthy(instance: NativeRemotePluginInstance, operation: String) {
        if (instance.state == InstanceState.ERROR)
            throw IllegalStateException("Remote plugin entered ERROR state during $operation")
    }

    private fun writeSilentAudioInputs(instance: NativeRemotePluginInstance, frameCount: Int) {
        val byteCount = frameCount * Float.SIZE_BYTES
        for (index in 0 until instance.getPortCount()) {
            val port = instance.getPort(index)
            if (port.content != PortInformation.PORT_CONTENT_TYPE_AUDIO ||
                port.direction != PortInformation.PORT_DIRECTION_INPUT)
                continue
            instance.setPortBuffer(index, directFloatBuffer(byteCount), byteCount)
        }
    }

    private fun validateFiniteAudioOutputs(pluginId: String, instance: NativeRemotePluginInstance, frameCount: Int, report: AapValidationReport) {
        val byteCount = frameCount * Float.SIZE_BYTES
        var outputCount = 0
        for (index in 0 until instance.getPortCount()) {
            val port = instance.getPort(index)
            if (port.content != PortInformation.PORT_CONTENT_TYPE_AUDIO ||
                port.direction != PortInformation.PORT_DIRECTION_OUTPUT)
                continue
            outputCount++
            val buffer = directFloatBuffer(byteCount)
            instance.getPortBuffer(index, buffer, byteCount)
            val samples = buffer.asFloatBuffer()
            var nonFiniteCount = 0
            var maxAbs = 0.0f
            while (samples.hasRemaining()) {
                val sample = samples.get()
                if (!sample.isFinite())
                    nonFiniteCount++
                else
                    maxAbs = maxOf(maxAbs, kotlin.math.abs(sample))
            }
            if (nonFiniteCount > 0) {
                report.fail(pluginId, "AAPVAL-AUDIO-001", "Audio output contains non-finite samples",
                    "Audio port $index produced $nonFiniteCount non-finite sample(s) in $frameCount frames.",
                    "Hosts cannot safely mix or process NaN/infinite audio.", "Ensure process() initializes and bounds all audio output samples.")
            } else {
                report.pass(pluginId, "AAPVAL-AUDIO-001", "Audio output is finite",
                    "Audio port $index produced $frameCount finite samples (peak=$maxAbs).")
            }
        }
        if (outputCount == 0)
            report.skip(pluginId, "AAPVAL-AUDIO-001", "No audio output ports declared", "The finite-audio check does not apply.")
    }

    private fun queueMidiNoteProbe(pluginId: String, instance: NativeRemotePluginInstance, report: AapValidationReport) {
        val hasMidiInput = (0 until instance.getPortCount()).any { index ->
            val port = instance.getPort(index)
            port.direction == PortInformation.PORT_DIRECTION_INPUT &&
                (port.content == PortInformation.PORT_CONTENT_TYPE_MIDI || port.content == PortInformation.PORT_CONTENT_TYPE_MIDI2)
        }
        if (!hasMidiInput) {
            report.skip(pluginId, "AAPVAL-MIDI-001", "No MIDI input ports declared", "The MIDI input probe does not apply.")
            return
        }
        // MIDI 2.0 Channel Voice note-on: group 0, channel 0, note 60, maximum velocity.
        val noteOn = directUmpBuffer(0x40903C00, 0xFFFF0000.toInt())
        instance.addEventUmpInput(noteOn, noteOn.capacity())
        assertHealthy(instance, "MIDI input")
        report.pass(pluginId, "AAPVAL-MIDI-001", "MIDI input was accepted", "Queued a MIDI 2.0 note-on for note 60.")
    }

    private fun queueParameterProbe(plugin: PluginInformation, instance: NativeRemotePluginInstance, report: AapValidationReport) {
        val pluginId = plugin.pluginId ?: return
        if (!declares(plugin, AudioPluginExtensionsBom.AAP_PARAMETERS_EXTENSION_URI_V4)) {
            report.skip(pluginId, "AAPVAL-PARAM-001", "Parameters extension is not declared", "The parameter transport check does not apply.")
            return
        }
        val count = instance.getParameterCount()
        if (count <= 0) {
            report.skip(pluginId, "AAPVAL-PARAM-001", "No runtime parameters", "The declared parameters extension exposed no runtime parameters.")
            return
        }
        val parameter = instance.getParameter(0)
        val target = when {
            parameter.maximumValue != parameter.defaultValue -> parameter.maximumValue
            parameter.minimumValue != parameter.defaultValue -> parameter.minimumValue
            else -> parameter.defaultValue
        }
        val words = UmpHelper.aapUmpSysex8ParameterPlain(
            parameter.id.toUInt(), parameter.minimumValue, parameter.maximumValue, target)
        instance.addEventUmpInput(directUmpBuffer(*words.toIntArray()), words.size * Int.SIZE_BYTES)
        assertHealthy(instance, "parameter input")
        report.pass(pluginId, "AAPVAL-PARAM-001", "Parameter automation was accepted",
            "Queued a valid parameter update for runtime parameter ${parameter.id}.")
    }

    private fun validateParameterExtension(plugin: PluginInformation, instance: NativeRemotePluginInstance, report: AapValidationReport) {
        val pluginId = plugin.pluginId ?: return
        if (!declares(plugin, AudioPluginExtensionsBom.AAP_PARAMETERS_EXTENSION_URI_V4)) return
        val runtimeCount = instance.getParameterCount()
        if (runtimeCount <= 0) return
        for (index in 0 until runtimeCount) {
            val parameter = instance.getParameter(index)
            val value = instance.getParameterValue(index)
            if (!value.isFinite() || value < parameter.minimumValue || value > parameter.maximumValue) {
                report.fail(pluginId, "AAPVAL-PARAM-002", "Runtime parameter value is invalid",
                    "Runtime parameter ${parameter.id} has value=$value outside ${parameter.minimumValue}..${parameter.maximumValue}.",
                    "Hosts cannot safely display or automate this parameter.",
                    "Return a finite parameter value within its declared range.")
            }
        }
        report.pass(pluginId, "AAPVAL-PARAM-002", "Runtime parameter values are valid",
            "Read $runtimeCount runtime parameter value(s).")
    }

    private fun validateStateExtension(plugin: PluginInformation, instance: NativeRemotePluginInstance, report: AapValidationReport) {
        val pluginId = plugin.pluginId ?: return
        if (!declares(plugin, AudioPluginExtensionsBom.AAP_STATE_EXTENSION_URI_V4)) {
            report.skip(pluginId, "AAPVAL-STATE-001", "State extension is not declared", "The state round-trip check does not apply.")
            return
        }
        val size = instance.getStateSize()
        when {
            size < 0 -> report.fail(pluginId, "AAPVAL-STATE-001", "State size is invalid", "Plugin returned state size $size.",
                "Hosts cannot allocate the state buffer.", "Return a non-negative state size.")
            size == 0 -> report.skip(pluginId, "AAPVAL-STATE-001", "Plugin has no serialised state", "The plugin returned an empty state buffer.")
            size > MAX_STATE_BYTES -> report.fail(pluginId, "AAPVAL-STATE-001", "State is too large for normal validation", "Plugin requested $size bytes.",
                "The validator avoids allocating unbounded state buffers.", "Keep normal preset/state data at or below $MAX_STATE_BYTES bytes, or investigate the reported size.")
            else -> {
                val state = ByteArray(size)
                instance.getState(state)
                instance.setState(state)
                assertHealthy(instance, "state round-trip")
                report.pass(pluginId, "AAPVAL-STATE-001", "State round-trip completed", "Read and restored $size state byte(s).")
            }
        }
    }

    private fun validatePresetExtension(plugin: PluginInformation, instance: NativeRemotePluginInstance, report: AapValidationReport) {
        val pluginId = plugin.pluginId ?: return
        if (!declares(plugin, AudioPluginExtensionsBom.AAP_PRESETS_EXTENSION_URI_V4)) {
            report.skip(pluginId, "AAPVAL-PRESET-001", "Presets extension is not declared", "The preset check does not apply.")
            return
        }
        val count = instance.getPresetCount()
        when {
            count < 0 -> report.fail(pluginId, "AAPVAL-PRESET-001", "Preset count is invalid", "Plugin returned preset count $count.",
                "Hosts cannot enumerate presets.", "Return a non-negative preset count.")
            count == 0 -> report.skip(pluginId, "AAPVAL-PRESET-001", "Plugin exposes no presets", "The preset selection check does not apply.")
            else -> {
                val name = instance.getPresetName(0)
                instance.setCurrentPresetIndex(0)
                assertHealthy(instance, "preset selection")
                report.pass(pluginId, "AAPVAL-PRESET-001", "Preset selection completed", "Selected preset 0 ('${name.ifBlank { "<unnamed>" }}') from $count preset(s).")
            }
        }
    }

    private fun declares(plugin: PluginInformation, extensionUri: String): Boolean =
        plugin.extensions.any { it.uri == extensionUri }

    private fun directFloatBuffer(byteCount: Int): ByteBuffer =
        ByteBuffer.allocateDirect(byteCount).order(ByteOrder.nativeOrder())

    private fun directUmpBuffer(vararg words: Int): ByteBuffer =
        ByteBuffer.allocateDirect(words.size * Int.SIZE_BYTES).order(ByteOrder.nativeOrder()).apply {
            words.forEach(::putInt)
            flip()
        }

    companion object {
        private const val DEFAULT_CONTROL_BYTES_PER_BLOCK = 0x10000
        private const val PROCESS_TIMEOUT_NANOSECONDS = 1_000_000_000L
        private const val MAX_STATE_BYTES = 1_048_576
    }
}
