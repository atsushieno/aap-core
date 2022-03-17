package org.androidaudioplugin.samples.host.engine

import android.content.Context
import android.media.AudioAttributes
import android.media.AudioFormat
import android.media.AudioTrack
import kotlinx.coroutines.GlobalScope
import kotlinx.coroutines.launch
import org.androidaudioplugin.*
import org.androidaudioplugin.hosting.AudioPluginHost
import org.androidaudioplugin.hosting.AudioPluginInstance
import java.nio.ByteBuffer
import java.nio.ByteOrder

const val PROCESS_AUDIO_NATIVE = false
const val FRAMES_PER_TICK = 100
// In this Plugin preview example engine, we don't really use the best sampling rate for the device
// as it only performs static audio processing and does not involve device audio inputs.
// We just process audio data based on 44100KHz, produce audio on the same rate, then play it as is.
const val PCM_DATA_SAMPLERATE = 44100

@ExperimentalUnsignedTypes
class PluginPreview(context: Context) {

    private var host : AudioPluginHost = AudioPluginHost(context.applicationContext)
    var instance : AudioPluginInstance? = null

    val inBuf : ByteArray
    val outBuf : ByteArray

    fun dispose() {
        track.release()
        host.dispose()
    }

    fun applyPlugin(service: PluginServiceInformation, plugin: PluginInformation, parametersOnUI: FloatArray?)
    {
        host.pluginInstantiatedListeners.clear()
        host.pluginInstantiatedListeners.add { instance ->
            assert(this.instance == null)
            this.instance = instance
            val floatCount = host.audioBufferSizeInBytes / 4 // 4 is sizeof(float)
            instance.prepare(floatCount)

            GlobalScope.launch {
                processPluginOnce(parametersOnUI)

                releasePluginInstance(instance)
            }
        }
        host.instantiatePlugin(plugin)

    }

    private fun processPluginOnce(parametersOnUI: FloatArray?) {
        val instance = this.instance!!
        val plugin = instance.pluginInfo
        val parameters = parametersOnUI ?: (0 until plugin.ports.count()).map { i -> plugin.ports[i].default }.toFloatArray()

        val midiSequence = MidiHelper.getMidiSequence()
        val midi1Events = MidiHelper.splitMidi1Events(midiSequence.toUByteArray())
        val midi1EventsGroups = MidiHelper.groupMidi1EventsByTiming(midi1Events).toList()

        // Kotlin version of audio/MIDI processing.

        var audioInL = -1
        var audioInR = -1
        var audioOutL = -1
        var audioOutR = -1
        var midiIn = -1
        instance.pluginInfo.ports.forEachIndexed { i, p ->
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
            } else if (p.content == PortInformation.PORT_CONTENT_TYPE_MIDI ||
                       p.content == PortInformation.PORT_CONTENT_TYPE_MIDI2) {
                if (p.direction == PortInformation.PORT_DIRECTION_INPUT)
                    midiIn = i
            }
        }

        val bufferFrameSize = host.audioBufferSizeInBytes / 4 // 4 is sizeof(float)

        (0 until plugin.ports.count()).map { i ->
            if (plugin.ports[i].direction == PortInformation.PORT_DIRECTION_OUTPUT)
                return@map
            if (plugin.ports[i].content == PortInformation.PORT_CONTENT_TYPE_AUDIO)
                return@map
            if (plugin.ports[i].content == PortInformation.PORT_CONTENT_TYPE_MIDI)
                return@map
            if (plugin.ports[i].content == PortInformation.PORT_CONTENT_TYPE_MIDI2)
                return@map
            else {
                val c = instance.getPortBuffer(i).order(ByteOrder.LITTLE_ENDIAN).asFloatBuffer()
                c.position(0)
                val v = parameters[i]
                (0 until bufferFrameSize).forEach { _ -> c.put(v) }
            }
        }

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
            deinterleaveInput(currentFrame, bufferFrameSize)

            if (audioInL >= 0) {
                val instanceInL = instance.getPortBuffer(audioInL)
                instanceInL.position(0)
                instanceInL.put(host.audioInputs[0], 0, bufferFrameSize * 4)
                if (audioInR > audioInL) {
                    val instanceInR = instance.getPortBuffer(audioInR)
                    instanceInR.position(0)
                    instanceInR.put(host.audioInputs[1], 0, bufferFrameSize * 4)
                }
            }
            if (midiIn >= 0) {
                // MIDI buffer is complicated. The AAP input MIDI buffer is formed as follows:
                // - i32 length unit specifier: positive frames, or negative frames per beat in the
                //   context tempo.
                // - i32 MIDI buffer size
                // - MIDI buffer contents in SMF-compatible format (but split in audio buffer)

                val midiBuffer = instance.getPortBuffer(midiIn).order(ByteOrder.LITTLE_ENDIAN)
                midiBuffer.position(0)
                midiBuffer.position(resetMidiBuffer(midiBuffer))
                var midiDataLengthInLoop = 0

                while (nextMidi1EventFrame < currentFrame + bufferFrameSize) {
                    val timedEvent = nextMidi1Group.first()
                    var deltaTimeTmp = MidiHelper.getFirstMidi1EventDuration(timedEvent)
                    var deltaTimeBytes = 1
                    while (deltaTimeTmp > 0x80) {
                        deltaTimeBytes++
                        deltaTimeTmp /= 0x80
                    }
                    val diffFrame = nextMidi1EventFrame % bufferFrameSize
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
            }

            instance.process()

            if (audioOutL >= 0) {
                val instanceOutL = instance.getPortBuffer(audioOutL)
                instanceOutL.position(0)
                instanceOutL.get(host.audioOutputs[0], 0, bufferFrameSize * 4)
                if (audioOutR > audioOutL) {
                    val instanceOutR = instance.getPortBuffer(audioOutR)
                    instanceOutR.position(0)
                    instanceOutR.get(host.audioOutputs[1], 0, bufferFrameSize * 4)
                } else {
                    // mono output
                    instanceOutL.position(0)
                    instanceOutL.get(host.audioOutputs[1], 0, bufferFrameSize * 4)
                }
            }

            interleaveOutput(currentFrame, bufferFrameSize)

            currentFrame += bufferFrameSize
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

        return 8
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

    val track: AudioTrack
    val audioTrackBufferSizeInBytes: Int

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

    private fun releasePluginInstance(instance: AudioPluginInstance) {
        instance.destroy()
        this.instance = null

        val serviceInfo = instance.service.serviceInfo
        host.serviceConnector.unbindAudioPluginService(serviceInfo.packageName)
    }

    init {
        val assets = context.applicationContext.assets
        val wavAsset = assets.open("sample.wav")
        inBuf = wavAsset.readBytes().drop(88).toByteArray()
        wavAsset.close()
        outBuf = ByteArray(inBuf.size)

        // set up AudioTrack
        // FIXME: once we support resampling then support platform audio configuration.
        val sampleRate = PCM_DATA_SAMPLERATE
        audioTrackBufferSizeInBytes = AudioTrack.getMinBufferSize(
            PCM_DATA_SAMPLERATE,
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
