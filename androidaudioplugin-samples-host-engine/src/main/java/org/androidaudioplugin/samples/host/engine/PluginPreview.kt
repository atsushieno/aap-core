package org.androidaudioplugin.samples.host.engine

import android.content.Context
import android.media.AudioAttributes
import android.media.AudioFormat
import android.media.AudioTrack
import dev.atsushieno.ktmidi.Ump
import dev.atsushieno.ktmidi.UmpFactory
import dev.atsushieno.ktmidi.toPlatformNativeBytes
import org.androidaudioplugin.*
import org.androidaudioplugin.hosting.AudioPluginClient
import org.androidaudioplugin.hosting.AudioPluginInstance
import java.nio.ByteBuffer
import java.nio.ByteOrder

class PluginPreview(context: Context) {

    companion object {
        const val FRAMES_PER_TICK = 100
        const val AUDIO_BUFFER_SIZE = 4096
        const val DEFAULT_CONTROL_BUFFER_SIZE = 4096
        // In this Plugin preview example engine, we don't really use the best sampling rate for the device
        // as it only performs static audio processing and does not involve device audio inputs.
        // We just process audio data based on 44100KHz, produce audio on the same rate, then play it as is.
        const val PCM_DATA_SAMPLE_RATE = 44100
    }

    private val host : AudioPluginClient = AudioPluginClient(context.applicationContext)
    private var instance: AudioPluginInstance? = null
    var pluginInfo: PluginInformation? = null
    private var lastError: Exception? = null

    val inBuf : ByteArray
    val outBuf : ByteArray

    // FIXME: maybe we don't need per-port allocation?
    private val audioProcessingBuffers = mutableListOf<ByteBuffer>()
    private val audioProcessingBufferSizesInBytes = mutableListOf<Int>()


    fun dispose() {
        track.release()
        host.dispose()
    }

    fun loadPlugin(pluginInfo: PluginInformation, callback: (AudioPluginInstance?, Exception?) ->Unit = {_,_ -> }) {
        if (instance != null) {
            callback(null, Exception("A plugin ${pluginInfo.pluginId} is already loaded"))
        } else {
            this.pluginInfo = pluginInfo
            host.instantiatePluginAsync(pluginInfo) { instance, error ->
                if (error != null) {
                    lastError = error
                    callback(null, error)
                } else if (instance != null) { // should be always true
                    this.instance = instance
                    instance.prepare(host.audioBufferSizeInBytes / 4, host.defaultControlBufferSizeInBytes)  // 4 is sizeof(float)
                    if (instance.proxyError != null) {
                        callback(null, instance.proxyError!!)
                    } else {
                        callback(instance, null)
                    }
                }
            }
        }
    }

    fun unloadPlugin() {
        val instance = this.instance
        if (instance != null) {
            if (instance.state == AudioPluginInstance.InstanceState.ACTIVE)
                instance.deactivate()
            instance.destroy()
        }

        host.serviceConnector.unbindAudioPluginService(instance!!.pluginInfo.packageName)
        pluginInfo = null
        this.instance = null
    }

    val instanceParameters : List<ParameterInformation>
        get() = if (instance == null) listOf() else (0 until instance!!.getParameterCount()).map { instance!!.getParameter(it) }

    val instancePorts : List<PortInformation>
        get() = if (instance == null) listOf() else (0 until instance!!.getPortCount()).map { instance!!.getPort(it) }

    fun applyPlugin(parametersOnUI: FloatArray?, errorCallback: (Exception?) ->Unit = {}) {
        val instance = this.instance ?: return

        outBuf.fill(0)
        audioProcessingBuffers.clear()
        audioProcessingBufferSizesInBytes.clear()
        (0 until instance.getPortCount()).forEach {
            val size = AUDIO_BUFFER_SIZE.coerceAtLeast(instance.getPort(it).minimumSizeInBytes)
            // Note that you will have to use allocateDirecT() here, otherwise ART crashes at accessing it on JNI.
            audioProcessingBuffers.add(ByteBuffer.allocateDirect(size))
            audioProcessingBufferSizesInBytes.add(size)
        }

        processPluginOnce(parametersOnUI)
        if (instance.proxyError != null)
            errorCallback(instance.proxyError!!)
    }

    private fun processPluginOnce(parametersOnUI: FloatArray?) {
        val instance = this.instance!!
        val parameters = parametersOnUI ?: (0 until instance.getParameterCount()).map { instance.getParameter(it).defaultValue .toFloat() }.toFloatArray()

        host.resetInputBuffers()
        host.resetOutputBuffers()
        host.resetControlBuffers()

        // Kotlin version of audio/MIDI processing.

        var audioInL = -1
        var audioInR = -1
        var audioOutL = -1
        var audioOutR = -1
        var midiIn = -1
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
            } else if (p.content == PortInformation.PORT_CONTENT_TYPE_MIDI) {
                if (p.direction == PortInformation.PORT_DIRECTION_INPUT)
                    midiIn = i
            } else if (p.content == PortInformation.PORT_CONTENT_TYPE_MIDI2) {
                if (p.direction == PortInformation.PORT_DIRECTION_INPUT)
                    midi2In = i
            }
        }

        val audioBufferFrameSize = host.audioBufferSizeInBytes / 4 // 4 is sizeof(float)
        val controlBufferFrameSize = host.defaultControlBufferSizeInBytes / 4 // 4 is sizeof(float)

        if (midi2In >= 0) {
            val midi2Bytes = mutableListOf<Byte>()

            (0 until instance.getParameterCount()).map { paraI ->
                val para = instance.getParameter(paraI)

                val ump = Ump(
                    UmpFactory.midi2NRPN(
                        0,
                        0,
                        para.id / 0x100,
                        para.id % 0x100,
                        parameters[paraI].toRawBits().toLong()
                    )
                )
                // generate Assignable Controllers into midi2Bytes.
                midi2Bytes.addAll(ump.toPlatformNativeBytes().toTypedArray())
            }

            val header = MidiHelper.toMidiBufferHeader(0, (parameters.size * 4).toUInt())
            midi2Bytes.addAll(0, header)

            val localBufferL = audioProcessingBuffers[midi2In]
            localBufferL.clear()
            localBufferL.put(midi2Bytes.toByteArray(), 0, midi2Bytes.size)
            instance.setPortBuffer(midi2In, localBufferL, midi2Bytes.size)
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

        val midiSequence = MidiHelper.getMidiSequence()
        val midi1Events = MidiHelper.splitMidi1Events(midiSequence.toUByteArray())
        val midi1EventsGroups = MidiHelper.groupMidi1EventsByTiming(midi1Events).toList()

        instance.activate()

        var currentFrame = 0
        val midi1EventsGroupsIterator = midi1EventsGroups.iterator()
        var nextMidi1Group = if (midi1EventsGroupsIterator.hasNext()) midi1EventsGroupsIterator.next() else listOf()
        val nextMidi1EventDeltaTime = if (nextMidi1Group.isNotEmpty()) MidiHelper.getFirstMidi1EventDuration(nextMidi1Group.first()) else 0
        var nextMidi1EventFrame = MidiHelper.expandSMPTE(FRAMES_PER_TICK, host.sampleRate, nextMidi1EventDeltaTime)

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
            if (midiIn >= 0) {
                // see aap-midi2.h for the header format (which should cover MIDI1 too).
                // MIDI buffer is complicated. The AAP input MIDI buffer is formed as follows:
                // - i32 length unit specifier: positive frames, or negative frames per beat in the
                //   context tempo.
                // - i32 MIDI buffer size
                // - 6 reserved i32 values
                // - MIDI buffer contents in SMF-compatible format (but split in audio buffer)

                val midiBuffer = audioProcessingBuffers[midiIn].order(ByteOrder.LITTLE_ENDIAN)
                midiBuffer.clear()
                midiBuffer.position(resetMidiBuffer(midiBuffer))
                var midiDataLengthInLoop = 0

                while (nextMidi1EventFrame < currentFrame + controlBufferFrameSize) {
                    val timedEvent = nextMidi1Group.first()
                    var deltaTimeTmp = MidiHelper.getFirstMidi1EventDuration(timedEvent)
                    var deltaTimeBytes = 1
                    while (deltaTimeTmp > 0x80) {
                        deltaTimeBytes++
                        deltaTimeTmp /= 0x80
                    }
                    val diffFrame = nextMidi1EventFrame % controlBufferFrameSize
                    val diffMTC = MidiHelper.toMidiTimeCode(FRAMES_PER_TICK, host.sampleRate, diffFrame)
                    val b0 = 0.toUByte()
                    val diffMTCLength = if (diffMTC[3] != b0) 4 else if (diffMTC[2] != b0) 3 else if (diffMTC[1] != b0) 2 else 1
                    val updatedFirstEvent = diffMTC.take(diffMTCLength).plus(timedEvent.drop(deltaTimeBytes))
                    val nextMidiEvents = updatedFirstEvent.plus(nextMidi1Group.drop(1).flatten()).map { u -> u.toByte() }.toByteArray()
                    midiDataLengthInLoop += nextMidiEvents.size
                    midiBuffer.put(nextMidiEvents)
                    if (!midi1EventsGroupsIterator.hasNext())
                        break
                    nextMidi1Group = midi1EventsGroupsIterator.next()
                    nextMidi1EventFrame += MidiHelper.expandSMPTE(FRAMES_PER_TICK, host.sampleRate, MidiHelper.getFirstMidi1EventDuration(nextMidi1Group.first()))
                }
                midiBuffer.position(4)
                midiBuffer.putInt(midiDataLengthInLoop)
                midiBuffer.position(0) // do we need this??
                instance.setPortBuffer(midiIn, midiBuffer, audioProcessingBufferSizesInBytes[midiIn])
            }

            instance.process()

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
    }

    private fun resetMidiBuffer(mb: ByteBuffer) : Int
    {
        val mbi = mb.asIntBuffer()
        val ticksPerFrame : Short = (-1 * FRAMES_PER_TICK).toShort()
        mbi.put(ticksPerFrame.toInt()) // 1 frame = 10 milliseconds
        mbi.put(0)

        return 32
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

    private val track: AudioTrack
    private val audioTrackBufferSizeInBytes: Int

    fun playSound(postApplied: Boolean)
    {
        val w = if(postApplied) outBuf else inBuf

        track.play()
        /* It is super annoying... there is no way to convert ByteBuffer to little-endian float array. I ended up to convert it manually here */
        val fa = FloatArray(w.size / 4)
        for (i in fa.indices) {
            val bits = (w[i * 4 + 3].toUByte().toInt() shl 24) + (w[i * 4 + 2].toUByte().toInt() shl 16) + (w[i * 4 + 1].toUByte().toInt() shl 8) + w[i * 4].toUByte().toInt()
            fa[i] = Float.fromBits(bits)
        }
        var i = 0
        while (i < w.size) {
            val ret = track.write(fa, i, audioTrackBufferSizeInBytes / 4, AudioTrack.WRITE_BLOCKING)
            if (ret <= 0)
                break
            i += ret
        }
        track.flush()
        track.stop()
    }

    init {
        host.audioBufferSizeInBytes = AUDIO_BUFFER_SIZE
        host.defaultControlBufferSizeInBytes = DEFAULT_CONTROL_BUFFER_SIZE

        val assets = context.applicationContext.assets
        val wavAsset = assets.open("sample.wav")
        inBuf = wavAsset.readBytes().drop(88).toByteArray()
        wavAsset.close()
        outBuf = ByteArray(inBuf.size)

        // set up AudioTrack
        // FIXME: once we support resampling then support platform audio configuration.
        val sampleRate = PCM_DATA_SAMPLE_RATE
        audioTrackBufferSizeInBytes = AudioTrack.getMinBufferSize(
            PCM_DATA_SAMPLE_RATE,
            AudioFormat.CHANNEL_OUT_STEREO,
            AudioFormat.ENCODING_PCM_FLOAT
        )
        track = AudioTrack.Builder()
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
            .build()
        track.setVolume(2.0f)

        val fa = FloatArray(1024)
        for (i in 0 .. 10)
            track.write(fa, 0, 1024, AudioTrack.WRITE_BLOCKING)
    }
}
