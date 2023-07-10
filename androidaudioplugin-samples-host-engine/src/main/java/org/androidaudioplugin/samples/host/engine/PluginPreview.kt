package org.androidaudioplugin.samples.host.engine

import android.content.Context
import android.media.AudioAttributes
import android.media.AudioFormat
import android.media.AudioTrack
import android.media.midi.MidiDeviceInfo.PortInfo
import android.media.midi.MidiInputPort
import android.media.midi.MidiManager
import android.os.Build
import android.util.Log
import android.view.View
import androidx.annotation.RequiresApi
import dev.atsushieno.ktmidi.*
import kotlinx.coroutines.*
import org.androidaudioplugin.ParameterInformation
import org.androidaudioplugin.PluginInformation
import org.androidaudioplugin.PortInformation
import org.androidaudioplugin.hosting.AudioPluginHostHelper
import org.androidaudioplugin.hosting.AudioPluginInstance
import org.androidaudioplugin.hosting.AudioPluginMidiSettings
import org.androidaudioplugin.hosting.AudioPluginSurfaceControlClient
import org.androidaudioplugin.hosting.UmpHelper
import java.nio.ByteBuffer
import java.nio.ByteOrder
import kotlin.coroutines.resume
import kotlin.coroutines.suspendCoroutine

class PluginPreview(private val context: Context) : AutoCloseable {
    private val LOG_TAG = "AAPPluginPreview"

    companion object {
        const val FRAMES_PER_TICK = 100
        const val AUDIO_BUFFER_SIZE = 4096
        const val DEFAULT_CONTROL_BUFFER_SIZE = 131072
        // In this Plugin preview example engine, we don't really use the best sampling rate for the device
        // as it only performs static audio processing and does not involve device audio inputs.
        // We just process audio data based on 44100KHz, produce audio on the same rate, then play it as is.
        const val PCM_DATA_SAMPLE_RATE = 44100
    }

    private val host : AudioPluginClient = AudioPluginClient(context.applicationContext)
    var instance: AudioPluginInstance? = null
    var guiInstanceId: Int? = null
    var pluginInfo: PluginInformation? = null
    private var lastError: Exception? = null
    var midiSettingsFlags: Int
        get() = instance?.getMidiMappingPolicy() ?: 0
        set(v) {
            // We can set the value only if the plugin is within current application
            assert(isPluginInCurrentApplication)
            AudioPluginMidiSettings.putMidiSettingsToSharedPreference(context, instance!!.pluginInfo.pluginId!!, v)
        }
    val isPluginInCurrentApplication
        get() = instance != null && instance!!.pluginInfo.packageName == context.packageName
    val inBuf : ByteArray
    val outBuf : ByteArray
    var selectedPresetIndex = -1

    // FIXME: maybe we don't need per-port allocation?
    private val audioProcessingBuffers = mutableListOf<ByteBuffer>()
    private val audioProcessingBufferSizesInBytes = mutableListOf<Int>()

    private var surfaceControlCached: AudioPluginSurfaceControlClient? = null
    val surfaceControl: AudioPluginSurfaceControlClient
        get() {
            if (surfaceControlCached == null)
                surfaceControlCached = AudioPluginHostHelper.createSurfaceControl(context)
            return surfaceControlCached!!
        }

    override fun close() {
        surfaceControlCached?.close()
        surfaceControlCached = null
        host.dispose()
    }

    suspend fun loadPlugin(pluginInfo: PluginInformation) : AudioPluginInstance {
        if (instance != null)
            // FIXME: use some dedicated type
            throw Exception("A plugin ${pluginInfo.pluginId} is already loaded")

        this.pluginInfo = pluginInfo

        host.connectToPluginService(pluginInfo.packageName)
        assert(this.instance == null) // avoid race condition

        val instance = host.instantiatePlugin(pluginInfo)
        instance.prepare(host.audioBufferSizeInBytes / 4, host.defaultControlBufferSizeInBytes)  // 4 is sizeof(float)
        if (instance.proxyError != null)
            throw instance.proxyError!!
        this.instance = instance
        return instance
    }

    fun unloadPlugin() {
        val instance = this.instance
        if (instance != null) {
            if (instance.state == AudioPluginInstance.InstanceState.ACTIVE)
                instance.deactivate()
            instance.destroy()

            host.disconnectPluginService(instance.pluginInfo.packageName)
            pluginInfo = null
            this.instance = null
        }
    }

    val instanceParameters : List<ParameterInformation>
        get() = if (instance == null) listOf() else (0 until instance!!.getParameterCount()).map { instance!!.getParameter(it) }

    val instancePorts : List<PortInformation>
        get() = if (instance == null) listOf() else (0 until instance!!.getPortCount()).map { instance!!.getPort(it) }

    val presetCount : Int
        get() = if (instance == null) 0 else instance!!.getPresetCount()

    fun getPresetName(index: Int) = if (instance == null) null else instance!!.getPresetName(index)

    fun applyPlugin(instanceParameterValues: FloatArray?, errorCallback: (Exception?) ->Unit = {}) {
        val instance = this.instance ?: return

        outBuf.fill(0)
        audioProcessingBuffers.clear()
        audioProcessingBufferSizesInBytes.clear()
        (0 until instance.getPortCount()).forEach {
            val baseSize =
                if (instance.getPort(it).content == PortInformation.PORT_CONTENT_TYPE_AUDIO) AUDIO_BUFFER_SIZE
                else host.defaultControlBufferSizeInBytes / 4 // 4 is sizeof(float)
            val size = baseSize.coerceAtLeast(instance.getPort(it).minimumSizeInBytes)
            // Note that you will have to use allocateDirecT() here, otherwise ART crashes at accessing it on JNI.
            audioProcessingBuffers.add(ByteBuffer.allocateDirect(size))
            audioProcessingBufferSizesInBytes.add(size)
        }

        processPluginOnce(instanceParameterValues)
        if (instance.proxyError != null)
            errorCallback(instance.proxyError!!)
    }

    private fun processPluginOnce(instanceParameterValues: FloatArray?) {
        val instance = this.instance!!
        val parameters = instanceParameterValues ?: (0 until instance.getParameterCount()).map { instance.getParameter(it).defaultValue .toFloat() }.toFloatArray()

        host.resetInputBuffers()
        host.resetOutputBuffers()
        host.resetControlBuffers()

        // Kotlin version of audio/MIDI processing.

        var audioInL = -1
        var audioInR = -1
        var audioOutL = -1
        var audioOutR = -1
        var midi2In = -1
        (0 until instance.getPortCount()).forEach { i ->
            val p = instance.getPort(i)
            if (p.content == PortInformation.PORT_CONTENT_TYPE_AUDIO) {
                if (p.direction == PortInformation.PORT_DIRECTION_INPUT) {
                    if (audioInL < 0)
                        audioInL = i
                    else
                        audioInR = i
                } else {
                    if (audioOutL < 0)
                        audioOutL = i
                    else
                        audioOutR = i
                }
            } else if (p.content == PortInformation.PORT_CONTENT_TYPE_MIDI2) {
                if (p.direction == PortInformation.PORT_DIRECTION_INPUT)
                    midi2In = i
            }
        }

        val audioBufferFrameSize = host.audioBufferSizeInBytes / 4 // 4 is sizeof(float)
        val controlBufferFrameSize = host.defaultControlBufferSizeInBytes / 4 // 4 is sizeof(float)

        val midi2Bytes = mutableListOf<Byte>()

        if (selectedPresetIndex >= 0)
            instance.setCurrentPresetIndex(selectedPresetIndex)
        else if (midi2In >= 0) {
            (instanceParameterValues?.indices)?.map { paraI ->
                val para = instanceParameters[paraI]
                val value = instanceParameterValues[paraI]
                // generate Sysex8 packet for Parameter Changes and push into midi2Bytes.
                // 7Eh, 7Fh, 0, 0, 7Fh, key, noteId, pIndex / 80h, pIndex % 80h, pVal1, pVal2, pVal3, pVal4
                val arr = UmpHelper.aapUmpSysex8Parameter(para.id.toUInt(), value, 0, 0, 0, 0u)
                val ump = Ump(arr[0], arr[1], arr[2], arr[3])
                midi2Bytes.addAll(ump.toPlatformNativeBytes().toList())
            }
        } else {
            // If there are parameter elements, look for ports based on each parameter's name.
            // If there isn't, just assume parameter index == port index.
            if (instance.getParameterCount() > 0) {
                (0 until instance.getParameterCount()).map { paraI ->
                    val para = instance.getParameter(paraI)
                    for (portI in 0 until instance.getPortCount()) {
                        if (para.name != instance.getPort(portI).name)
                            continue
                        val c = audioProcessingBuffers[portI].order(ByteOrder.LITTLE_ENDIAN).asFloatBuffer()
                        c.position(0)
                        // just put one float value
                        c.put(parameters[paraI])
                        instance.setPortBuffer(
                            portI,
                            audioProcessingBuffers[portI],
                            audioProcessingBufferSizesInBytes[portI]
                        )
                        break
                    }
                }
            } else {
                for (portI in parameters.indices) {
                    val c =
                        audioProcessingBuffers[portI].order(ByteOrder.LITTLE_ENDIAN).asFloatBuffer()
                    c.position(0)
                    // just put one float value
                    c.put(parameters[portI])
                    instance.setPortBuffer(
                        portI,
                        audioProcessingBuffers[portI],
                        audioProcessingBufferSizesInBytes[portI]
                    )
                    break
                }
            }
        }

        instance.activate()

        var currentFrame = 0
        var cycle = 0
        var noteStateOn1 = false
        var noteStateOn2 = false

        // We process audio and MIDI buffers in this loop, until currentFrame reaches the end of
        // the input sample data. Note that it does not involve any real tiem processing.
        // For such usage, there should be native audio callback based on Oboe or AAudio
        // (not in Kotlin).
        while (currentFrame < inBuf.size / 2 / 4) { // channels / sizeof(float)
            deinterleaveInput(currentFrame, audioBufferFrameSize)

            if (audioInL >= 0) {
                val localBufferL = audioProcessingBuffers[audioInL]
                localBufferL.clear()
                localBufferL.put(host.audioInputs[0], 0, audioBufferFrameSize * 4)
                instance.setPortBuffer(audioInL, localBufferL, audioProcessingBufferSizesInBytes[audioInL])
                if (audioInR > audioInL) {
                    val localBufferR = audioProcessingBuffers[audioInR]
                    localBufferR.clear()
                    localBufferR.put(host.audioInputs[1], 0, audioBufferFrameSize * 4)
                    instance.setPortBuffer(audioInR, localBufferR, audioProcessingBufferSizesInBytes[audioInR])
                }
            }
            if (midi2In >= 0) {
                // Insert note events to the MIDI2 buffer (which may or may not contain parameter NRPNs)
                //
                // On MIDI2 channel, it plays n64 notes on channel #0, for 24 cycles long, per 32 cycles,
                // and after 3 of them it also plays n70, on channel #1, for 40 cycles long, per 48 cycles.
                // Both are required to finish at note off state (thus asserts at last)
                if (cycle % 32 == 4 && cycle < 32 * 6) {
                    val noteOn = Ump(UmpFactory.midi2NoteOn(
                        0, 0, 64, 0, 0xF800, 0))
                    midi2Bytes.addAll(noteOn.toPlatformNativeBytes().toTypedArray())
                    noteStateOn1 = true
                }
                if (cycle % 32 == 28 && cycle <= 32 * 6) {
                    val noteOff = Ump(UmpFactory.midi2NoteOff(
                        0, 0, 64, 0, 0, 0))
                    midi2Bytes.addAll(noteOff.toPlatformNativeBytes().toTypedArray())
                    noteStateOn1 = false
                }
                if (cycle > 32 * 3 && cycle % 48 == 4 && cycle < 32 * 3 + 48 * 2) {
                    val noteOn = Ump(UmpFactory.midi2NoteOn(
                        0, 1, 70, 0, 0xF800, 0))
                    midi2Bytes.addAll(noteOn.toPlatformNativeBytes().toTypedArray())
                    noteStateOn2 = true
                }
                if (cycle > 32 * 3 && cycle % 48 == 44 && cycle <= 32 * 3 + 48 * 2) {
                    val noteOff = Ump(UmpFactory.midi2NoteOff(
                        0, 1, 70, 0, 0, 0))
                    midi2Bytes.addAll(noteOff.toPlatformNativeBytes().toTypedArray())
                    noteStateOn2 = false
                }
                cycle++

                // setup AAP buffer for this cycle
                val header = MidiHelper.toMidiBufferHeader(0, (midi2Bytes.size).toUInt())
                midi2Bytes.addAll(0, header)

                val localBuffer = audioProcessingBuffers[midi2In]
                localBuffer.clear()
                localBuffer.put(midi2Bytes.toByteArray(), 0, midi2Bytes.size)
                instance.setPortBuffer(midi2In, localBuffer, midi2Bytes.size)
            }

            instance.process(audioBufferFrameSize)

            if (midi2In >= 0)
                midi2Bytes.clear()

            if (audioOutL >= 0) {
                val localBufferL = audioProcessingBuffers[audioOutL]
                instance.getPortBuffer(audioOutL, localBufferL, audioProcessingBufferSizesInBytes[audioOutL])
                localBufferL.position(0)
                localBufferL.get(host.audioOutputs[0], 0, audioBufferFrameSize * 4)
                if (audioOutR >= 0) {
                    val localBufferR = audioProcessingBuffers[audioOutR]
                    instance.getPortBuffer(audioOutR, localBufferR, audioProcessingBufferSizesInBytes[audioOutR])
                    localBufferR.position(0)
                    localBufferR.get(host.audioOutputs[1], 0, audioBufferFrameSize * 4)
                } else {
                    // mono output - copy plugin L output to host R output.
                    localBufferL.position(0)
                    localBufferL.get(host.audioOutputs[1], 0, audioBufferFrameSize * 4)
                }
            }

            interleaveOutput(currentFrame, audioBufferFrameSize)

            currentFrame += audioBufferFrameSize
        }

        instance.deactivate()

        processAudioCompleted()

        assert(!noteStateOn1)
        assert(!noteStateOn2)
    }

    var processAudioCompleted : () -> Unit = {}

    private fun deinterleaveInput(startInFloat: Int, sizeInFloat: Int)
    {
        val l = ByteBuffer.wrap(host.audioInputs[0]).order(ByteOrder.LITTLE_ENDIAN).asFloatBuffer()
        val r = ByteBuffer.wrap(host.audioInputs[1]).order(ByteOrder.LITTLE_ENDIAN).asFloatBuffer()
        val inF = ByteBuffer.wrap(inBuf).order(ByteOrder.LITTLE_ENDIAN).asFloatBuffer()
        inF.position(startInFloat * 2)
        var j = startInFloat * 2
        val end = inBuf.size / 4
        for (i in 0 until sizeInFloat) {
            if (j >= end)
                break
            l.put(inF[j++])
            r.put(inF[j++])
        }
    }

    private fun interleaveOutput(startInFloat: Int, sizeInFloat: Int)
    {
        val outL = ByteBuffer.wrap(host.audioOutputs[0]).order(ByteOrder.LITTLE_ENDIAN).asFloatBuffer()
        val outR = ByteBuffer.wrap(host.audioOutputs[1]).order(ByteOrder.LITTLE_ENDIAN).asFloatBuffer()
        val outF = ByteBuffer.wrap(outBuf).order(ByteOrder.LITTLE_ENDIAN).asFloatBuffer()
        outF.position(startInFloat * 2)
        var j = startInFloat * 2
        val end = outBuf.size / 4
        for (i in 0 until sizeInFloat) {
            if (j >= end)
                break
            outF.put(outL[i])
            outF.put(outR[i])
            j += 2
        }
    }

    fun setupAudioTrack() : Pair<AudioTrack,Int>
    {
        // set up AudioTrack
        // FIXME: once we support resampling then support platform audio configuration.
        val sampleRate = PCM_DATA_SAMPLE_RATE
        val audioTrackBufferSizeInBytes = AudioTrack.getMinBufferSize(
            PCM_DATA_SAMPLE_RATE,
            AudioFormat.CHANNEL_OUT_STEREO,
            AudioFormat.ENCODING_PCM_FLOAT
        )
        val track = AudioTrack.Builder()
            .setAudioAttributes(
                AudioAttributes.Builder()
                    .setUsage(AudioAttributes.USAGE_MEDIA)
                    .setContentType(AudioAttributes.CONTENT_TYPE_MUSIC)
                    .build()
            )
            .setAudioFormat(
                AudioFormat.Builder()
                    .setEncoding(AudioFormat.ENCODING_PCM_FLOAT)
                    .setSampleRate(sampleRate)
                    .setChannelMask(AudioFormat.CHANNEL_OUT_STEREO)
                    .build()
            )
            .setBufferSizeInBytes(audioTrackBufferSizeInBytes)
            .setTransferMode(AudioTrack.MODE_STREAM)
            .setPerformanceMode(AudioTrack.PERFORMANCE_MODE_LOW_LATENCY)
            .build()
        track.setVolume(2.0f)

        val fa = FloatArray(1024)
        for (i in 0 .. 10)
            track.write(fa, 0, 1024, AudioTrack.WRITE_BLOCKING)
        return Pair(track, audioTrackBufferSizeInBytes)
    }

    fun playSound(postApplied: Boolean)
    {
        val w = if(postApplied) outBuf else inBuf

        val (track, audioTrackBufferSizeInBytes) = setupAudioTrack()

        track.play()

        // It is somewhat annoying...
        val fb = ByteBuffer.wrap(w).order(ByteOrder.LITTLE_ENDIAN).asFloatBuffer()
        val fa = FloatArray(w.size / 4)
        for (i in fa.indices)
            fa[i] = fb[i]

        var i = 0
        while (i < fa.size) {
            val ret = track.write(fa, i, audioTrackBufferSizeInBytes / 4, AudioTrack.WRITE_BLOCKING)
            if (ret <= 0)
                break
            i += ret
        }
        track.flush()
        track.stop()
        track.release()
    }

    private var midi_input: MidiInputPort? = null

    fun playMidiNotes() {
        CoroutineScope(Dispatchers.Default).launch { doPlayMidiNotes() }
    }

    fun stopMidiService() {
        synchronized(this) {
            midi_input?.close()
            midi_input = null
        }
    }

    private suspend fun openMidiDevice(): MidiInputPort = suspendCoroutine { continuation ->
        val manager = context.getSystemService(Context.MIDI_SERVICE) as MidiManager
        val deviceInfo = manager.devices.first { deviceInfo ->
            val serviceInfo = deviceInfo.properties.get("service_info") as android.content.pm.ServiceInfo?
            val pluginInfo = this.pluginInfo
            serviceInfo != null && pluginInfo != null && serviceInfo.packageName == pluginInfo.packageName
        }
        manager.openDevice(deviceInfo, { device ->
            val port = deviceInfo.ports.firstOrNull {
                it.type == PortInfo.TYPE_INPUT &&
                        it.name == pluginInfo?.displayName
            } ?: deviceInfo.ports.firstOrNull {
                it.type == PortInfo.TYPE_INPUT
            }
            val portNumber = port?.portNumber ?: return@openDevice
            synchronized(this) {
                continuation.resume(device.openInputPort(portNumber))
            }
        }, null)
    }

    private var midi_play_count : Byte = 0
    private var midi_play_busy = false

    private fun noteNumber(oct: Int, n: Byte) = (oct * 12 + n).toByte()

    private suspend fun doPlayMidiNotes() {
        val oct = midi_play_count % 4
        if (midi_play_busy)
            return
        midi_play_busy = true
        if (midi_input == null)
            midi_input = openMidiDevice()
        try {
            midi_input?.send(
                byteArrayOf(
                    0x90.toByte(), noteNumber(oct, 0x30), 0x78,
                    0x91.toByte(), noteNumber(oct, 0x34), 0x78,
                    0x92.toByte(), noteNumber(oct, 0x37), 0x78,
                ), 0, 9
            )
            delay(1000)
            midi_input?.send(
                byteArrayOf(
                    0x80.toByte(), noteNumber(oct, 0x30), 0,
                    0x81.toByte(), noteNumber(oct, 0x34), 0,
                    0x82.toByte(), noteNumber(oct, 0x37), 0,
                ), 0, 9
            )
            midi_play_count++
        } catch (ex: Exception) {
            Log.e(LOG_TAG, "AAPMidiDeviceService client caught an error: " + ex.stackTraceToString())
            lastError = ex
        }
        midi_play_busy = false
    }

    fun showGui() {
        guiInstanceId = instance?.createGui()
        instance?.showGui(guiInstanceId!!)
    }

    fun hideGui() {
        if (guiInstanceId != null) {
            instance?.hideGui(guiInstanceId!!)
            instance?.destroyGui(guiInstanceId!!)
            guiInstanceId = null
        }
    }

    @OptIn(DelicateCoroutinesApi::class)
    fun connectSurfaceControlUI(width: Int, height: Int) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            surfaceControl.apply {
                GlobalScope.launch {
                    connectUI(instance!!, width, height)
                }
            }
        }
    }

    fun showSurfaceGui() {
        surfaceControl.surfaceView.visibility = View.VISIBLE
    }

    fun hideSurfaceGui() {
        surfaceControl.surfaceView.visibility = View.GONE
    }

    init {
        host.audioBufferSizeInBytes = AUDIO_BUFFER_SIZE
        host.defaultControlBufferSizeInBytes = DEFAULT_CONTROL_BUFFER_SIZE

        val assets = context.applicationContext.assets
        val wavAsset = assets.open("sample.wav")
        inBuf = wavAsset.readBytes().drop(88).toByteArray()
        wavAsset.close()
        outBuf = ByteArray(inBuf.size)
    }
}
