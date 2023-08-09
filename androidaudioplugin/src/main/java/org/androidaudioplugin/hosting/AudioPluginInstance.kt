package org.androidaudioplugin.hosting

import android.os.RemoteException
import android.util.Log
import org.androidaudioplugin.*
import java.nio.ByteBuffer


/*
  AudioPluginInstance implementation that is based on native `RemotePluginInstance`.
 */
class AudioPluginInstance internal constructor(
    val native : NativeRemotePluginInstance,
    private val onDestroy: (instance: AudioPluginInstance) -> Unit,
    val pluginInfo: PluginInformation) {

    companion object {
        fun create(client: NativePluginClient,
                   onDestroy: (instance: AudioPluginInstance) -> Unit,
                   pluginInfo: PluginInformation,
                   sampleRate: Int) : AudioPluginInstance {
            val native = client.createInstanceFromExistingConnection(sampleRate, pluginInfo.pluginId!!)
            return AudioPluginInstance(native, onDestroy, pluginInfo)
        }
    }

    enum class InstanceState {
        UNPREPARED,
        INACTIVE,
        ACTIVE,
        DESTROYED,
        ERROR
    }

    var state = InstanceState.UNPREPARED

    var proxyError: Exception? = null

    val instanceId: Int
        get() = native.instanceId

    private fun runCatchingRemoteException(func: () -> Unit) {
        runCatchingRemoteExceptionFor(Unit) {
            func()
        }
    }

    private fun <T> runCatchingRemoteExceptionFor(default: T, func: () -> T) : T {
        if (state == InstanceState.ERROR)
            return default
        return try {
            func()
        } catch (ex: RemoteException) {
            state = InstanceState.ERROR
            proxyError = ex
            Log.e("AAP", "AudioPluginInstance received RemoteException: ${ex.message ?: ""}\n{${ex.stackTraceToString()}")
            default
        }
    }

    fun prepare(audioSamplesPerBlock: Int, defaultControlBytesPerBlock: Int) {
        runCatchingRemoteException {
            native.prepare(audioSamplesPerBlock, defaultControlBytesPerBlock)
            state = InstanceState.INACTIVE
        }
    }

    fun activate() {
        runCatchingRemoteException {
            native.activate()

            state = InstanceState.ACTIVE
        }
    }

    fun deactivate() {
        runCatchingRemoteException {
            native.deactivate()

            state = InstanceState.INACTIVE
        }
    }

    fun process(frameCount: Int, timeoutInNanoseconds: Long = 0) {
        runCatchingRemoteException {
            native.process(frameCount, timeoutInNanoseconds)
        }
    }

    fun destroy() {
        if (state == InstanceState.ACTIVE)
            deactivate()
        if (state != InstanceState.DESTROYED) {
            try {
                native.destroy()
            } catch (ex: Exception) {
                // nothing we can do here
            }
            try {
                onDestroy(this)
            } catch (ex: Exception) {
                // nothing we can do here
            }
            state = InstanceState.DESTROYED
        }
    }

    // parameter/port/buffer manipulation
    fun getParameterCount() = native.getParameterCount()
    fun getParameter(index: Int) = native.getParameter(index)
    fun getPortCount() = native.getPortCount()
    fun getPort(index: Int) = native.getPort(index)
    fun getPortBuffer(portIndex: Int, buffer: ByteBuffer, size: Int) = native.getPortBuffer(portIndex, buffer, size)
    fun setPortBuffer(portIndex: Int, buffer: ByteBuffer, size: Int) = native.setPortBuffer(portIndex, buffer, size)
    fun getMidiMappingPolicy() = native.getMidiMappingPolicy()

    // Standard Extensions

    fun getStateSize() : Int = native.getStateSize()
    fun getState(data: ByteArray) = native.getState(data)
    fun setState(data: ByteArray) = native.setState(data)

    fun getPresetCount() = native.getPresetCount()
    fun getCurrentPresetIndex() = native.getCurrentPresetIndex()
    fun setCurrentPresetIndex(index: Int) = native.setCurrentPresetIndex(index)
    fun getPresetName(index: Int) = native.getPresetName(index)

    fun createGui() = native.createGui()
    fun showGui(guiInstanceId: Int) = native.showGui(guiInstanceId)
    fun hideGui(guiInstanceId: Int) = native.hideGui(guiInstanceId)
    fun resizeGui(guiInstanceId: Int, width: Int, height: Int) = native.resizeGui(guiInstanceId, width, height)
    fun destroyGui(guiInstanceId: Int) = native.destroyGui(guiInstanceId)

    fun addEventUmpInput(data: ByteBuffer, size: Int) = native.addEventUmpInput(data, size)
}
