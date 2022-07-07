package org.androidaudioplugin.hosting

import android.content.Context
import java.lang.UnsupportedOperationException

/** This class not to endorse any official API for hosting AAP.
 * Its implementation is hacky and not really with decent API design.
 * It is to provide usable utilities for plugin developers as a proof of concept.
 */
class AudioPluginClient(private var applicationContext: Context)
    : AudioPluginClientBase(applicationContext) {

    // Audio buses and buffers management

    var audioInputs = mutableListOf<ByteArray>()
    var controlInputs = mutableListOf<ByteArray>()
    var audioOutputs = mutableListOf<ByteArray>()

    private var isActive = false

    var audioBufferSizeInBytes = 4096 * 4
        set(value) {
            field = value
            resetInputBuffer()
            resetOutputBuffer()
        }
    var defaultControlBufferSizeInBytes = 4096 * 4
        set(value) {
            field = value
            resetControlBuffer()
        }

    private fun throwIfRunning() { if (isActive) throw UnsupportedOperationException("AudioPluginHost is already running") }

    var inputAudioBus = AudioBusPresets.stereo // it will be initialized at init() too for allocating buffers.
        set(value) {
            throwIfRunning()
            field = value
            resetInputBuffer()
        }
    private fun resetInputBuffer() = expandBufferArrays(audioInputs, inputAudioBus.map.size, audioBufferSizeInBytes)

    var inputControlBus = AudioBusPresets.mono
        set(value) {
            throwIfRunning()
            field = value
            resetControlBuffer()
        }
    private fun resetControlBuffer() = expandBufferArrays(controlInputs, inputControlBus.map.size, defaultControlBufferSizeInBytes)

    var outputAudioBus = AudioBusPresets.stereo // it will be initialized at init() too for allocating buffers.
        set(value) {
            throwIfRunning()
            field = value
            resetOutputBuffer()
        }
    private fun resetOutputBuffer() = expandBufferArrays(audioOutputs, outputAudioBus.map.size, audioBufferSizeInBytes)

    private fun expandBufferArrays(list : MutableList<ByteArray>, newSize : Int, bufferSize : Int) {
        if (newSize > list.size)
            (0 until newSize - list.size).forEach {list.add(ByteArray(bufferSize)) }
        while (newSize < list.size)
            list.remove(list.last())
        for (i in 0 until newSize)
            if (list[i].size < bufferSize)
                list[i] = ByteArray(bufferSize)
    }

    init {
        inputAudioBus = AudioBusPresets.stereo
        outputAudioBus = AudioBusPresets.stereo
    }
}
