package org.androidaudioplugin.ui.compose.app

import android.content.Context
import android.content.pm.ServiceInfo
import android.media.midi.MidiDeviceInfo
import android.media.midi.MidiInputPort
import android.media.midi.MidiManager
import android.util.Log
import androidx.compose.material.Button
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.toMutableStateList
import kotlinx.coroutines.launch
import org.androidaudioplugin.PluginInformation
import org.androidaudioplugin.composeaudiocontrols.DiatonicKeyboardNoteExpressionOrigin
import org.androidaudioplugin.composeaudiocontrols.DiatonicKeyboardWithControllers
import kotlin.coroutines.resume
import kotlin.coroutines.suspendCoroutine
import kotlin.math.max
import kotlin.math.min
import kotlin.math.roundToInt


class MidiPlaygroundBackend(private val context: Context, private val logTag: String, private val pluginInfo: PluginInformation) {

    private var _lastError: Exception? = null

    val lastError: Exception?
        get() = _lastError

    private var midi_input: MidiInputPort? = null

    fun stopMidiService() {
        synchronized(this) {
            midi_input?.close()
            midi_input = null
        }
    }

    private suspend fun openMidiDevice(): MidiInputPort = suspendCoroutine { continuation ->
        val manager = context.getSystemService(Context.MIDI_SERVICE) as MidiManager
        val deviceInfo = manager.devices.first { deviceInfo ->
            val serviceInfo = deviceInfo.properties.get("service_info") as ServiceInfo?
            serviceInfo != null && serviceInfo.packageName == pluginInfo.packageName
        }
        manager.openDevice(deviceInfo, { device ->
            val port = deviceInfo.ports.firstOrNull {
                it.type == MidiDeviceInfo.PortInfo.TYPE_INPUT &&
                        it.name == pluginInfo?.displayName
            } ?: deviceInfo.ports.firstOrNull {
                it.type == MidiDeviceInfo.PortInfo.TYPE_INPUT
            }
            val portNumber = port?.portNumber ?: return@openDevice
            synchronized(this) {
                continuation.resume(device.openInputPort(portNumber))
            }
        }, null)
    }

    suspend fun sendMidi1Bytes(bytes: ByteArray) {
        if (midi_input == null)
            midi_input = openMidiDevice()
        try {
            midi_input?.send(bytes, 0, bytes.size)
        } catch (ex: Exception) {
            Log.e(logTag, "MidiDeviceService client caught an error: " + ex.stackTraceToString())
            _lastError = ex
        }
    }
}

@Composable
fun MidiDeviceServicePlayground(context: PluginManagerContext, plugin: PluginInformation) {
    if (!(plugin.category?: "").contains("Instrument"))
        return

    val coroutineScope = rememberCoroutineScope()
    val engine by remember { mutableStateOf(MidiPlaygroundBackend(context.context, context.logTag, plugin)) }
    val noteOnStates = remember { List(128) { 0L }.toMutableStateList() }
    DiatonicKeyboardWithControllers(noteOnStates,
        onNoteOn = { note, v ->
            coroutineScope.launch {
                engine.sendMidi1Bytes(byteArrayOf(0x90.toByte(), note.toByte(), 0x78)) }
            noteOnStates[note] = v
        },
        onNoteOff = { note, vel ->
            coroutineScope.launch {
                engine.sendMidi1Bytes(byteArrayOf(0x80.toByte(), note.toByte(), 0)) }
            noteOnStates[note] = 0L
        },
        onExpression = { origin, note, data ->
            coroutineScope.launch {
                when (origin) {
                    DiatonicKeyboardNoteExpressionOrigin.HorizontalDragging -> {
                        // Pitchbend
                        val data14 = max(min(data * 8192, 8191f), -8192f).roundToInt()
                        engine.sendMidi1Bytes(byteArrayOf(0xE0.toByte(), (data14 / 128).toByte(), (data14 % 128).toByte()))
                    }

                    DiatonicKeyboardNoteExpressionOrigin.VerticalDragging ->
                        // Expression
                        engine.sendMidi1Bytes(byteArrayOf(0xB0.toByte(), 7, min(data * 128, 127f).roundToInt().toByte()))

                    DiatonicKeyboardNoteExpressionOrigin.Pressure ->
                        // PAf
                        engine.sendMidi1Bytes(byteArrayOf(0xA0.toByte(), note.toByte(), min(data * 128, 127f).roundToInt().toByte())) }
            }
        }
    )

    Button(enabled = plugin.category?.contains("Instrument") == true,
        onClick = {
            coroutineScope.launch { engine.stopMidiService() }
        }) {
        Text("Stop MIDI")
    }
}
