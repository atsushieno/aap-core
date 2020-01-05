package org.androidaudioplugin

import android.content.Context
import android.os.IBinder
import java.lang.UnsupportedOperationException

// This class is not to endorse any official API for hosting AAP.
// Its implementation is hacky and not really with decent API design.
// It is to provide usable utilities for for developers as a proof of concept.
class AudioPluginHost(context: Context) {
    companion object {
        init {
            System.loadLibrary("androidaudioplugin")
        }

        @JvmStatic
        external fun setApplicationContext(applicationContext: Context)

        @JvmStatic
        external fun initialize(pluginInfos: Array<PluginInformation>)
    }

    var connectedBinders = mutableListOf<IBinder>()

    fun addToConnections(binder: IBinder) {
        if (connectedBinders.any { c -> c.interfaceDescriptor == binder.interfaceDescriptor })
            return
        connectedBinders.add(binder)
    }

    fun removeFromConnections(binder: IBinder) {
        var existing = connectedBinders.firstOrNull { c -> c.interfaceDescriptor == binder.interfaceDescriptor }
        if (existing == null)
            return
    }

    // Audio buses and buffers management

    var audioInputs = mutableListOf<ByteArray>()
    var controlInputs = mutableListOf<ByteArray>()
    var audioOutputs = mutableListOf<ByteArray>()

    var isConfigured = false
    var isActive = false

    var sampleRate = 44100

    var audioBufferSize = 44100
        set(value) {
            field = value
            resetInputBuffer()
            resetOutputBuffer()
        }
    var controlBufferSize = 44100
        set(value) {
            field = value
            resetControlBuffer()
        }

    fun throwIfRunning() { if (isActive) throw UnsupportedOperationException("AudioPluginHost is already running") }

    var inputAudioBus = AudioBusPresets.stereo // it will be initialized at init() too for allocating buffers.
        set(value) {
            throwIfRunning()
            field = value
            resetInputBuffer()
        }
    fun resetInputBuffer() = expandBufferArrays(audioInputs, inputAudioBus.map.size, audioBufferSize)

    var inputControlBus = AudioBusPresets.monoral
        set(value) {
            throwIfRunning()
            field = value
            resetControlBuffer()
        }
    fun resetControlBuffer() = expandBufferArrays(controlInputs, inputControlBus.map.size, controlBufferSize)

    var outputAudioBus = AudioBusPresets.stereo // it will be initialized at init() too for allocating buffers.
        set(value) {
            throwIfRunning()
            field = value
            resetOutputBuffer()
        }
    fun resetOutputBuffer() = expandBufferArrays(audioOutputs, outputAudioBus.map.size, audioBufferSize)

    fun expandBufferArrays(list : MutableList<ByteArray>, newSize : Int, bufferSize : Int) {
        if (newSize > list.size)
            (0 until newSize - list.size).forEach {list.add(ByteArray(bufferSize)) }
        while (newSize < list.size)
            list.remove(list.last())
    }

    init {
        setApplicationContext(context)
        initialize(AudioPluginHostHelper.queryAudioPluginServices(context).flatMap { s -> s.plugins }.toTypedArray())
        inputAudioBus = AudioBusPresets.stereo
        outputAudioBus = AudioBusPresets.stereo
    }
}

class AudioBus(var name : String, var map : Map<String,Int>)

class AudioBusPresets
{
    companion object {
        val monoral = AudioBus("Monoral", mapOf(Pair("center", 0)))
        val stereo = AudioBus("Stereo", mapOf(Pair("Left", 0), Pair("Right", 1)))
        val surround50 = AudioBus(
            "5.0 Surrounded", mapOf(
                Pair("Left", 0), Pair("Center", 1), Pair("Right", 2),
                Pair("RearLeft", 3), Pair("RearRight", 4)
            )
        )
        val surround51 = AudioBus(
            "5.1 Surrounded", mapOf(
                Pair("Left", 0), Pair("Center", 1), Pair("Right", 2),
                Pair("RearLeft", 3), Pair("RearRight", 4), Pair("LowFrequencyEffect", 5)
            )
        )
        val surround61 = AudioBus(
            "6.1 Surrounded", mapOf(
                Pair("Left", 0), Pair("Center", 1), Pair("Right", 2),
                Pair("SideLeft", 3), Pair("SideRight", 4),
                Pair("RearCenter", 5), Pair("LowFrequencyEffect", 6)
            )
        )
        val surround71 = AudioBus(
            "7.1 Surrounded", mapOf(
                Pair("Left", 0), Pair("Center", 1), Pair("Right", 2),
                Pair("SideLeft", 3), Pair("SideRight", 4),
                Pair("RearLeft", 5), Pair("RearRight", 6), Pair("LowFrequencyEffect", 7)
            )
        )
    }
}
