package org.androidaudioplugin.hosting

import android.os.ParcelFileDescriptor
import android.os.RemoteException
import android.os.SharedMemory
import android.util.Log
import org.androidaudioplugin.*
import org.androidaudioplugin.AudioPluginNatives
import java.nio.ByteBuffer


/*
  AudioPluginInstance implementation that is based on native `RemotePluginInstance`.
 */
class AudioPluginInstance internal constructor(
    serviceConnector: AudioPluginServiceConnector,
    conn: PluginServiceConnection,
    var pluginInfo: PluginInformation,
    sampleRate: Int) {

    enum class InstanceState {
        UNPREPARED,
        INACTIVE,
        ACTIVE,
        DESTROYED,
        ERROR
    }

    private val client = NativePluginClient(sampleRate, serviceConnector)
    var state = InstanceState.UNPREPARED

    private class SharedMemoryBuffer (var shm : SharedMemory, var fd : Int, var buffer: ByteBuffer)
    private var shmList = mutableListOf<SharedMemoryBuffer>()
    private val proxy : NativeRemotePluginInstance

    var proxyError: Exception? = null

    init {
        if (!client.serviceConnector.connectedServices.contains(conn))
            client.serviceConnector.connectedServices.add(conn)
        proxy = client.createInstanceFromExistingConnection(pluginInfo.pluginId!!)
    }

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

    fun prepare(audioSamplesPerBlock: Int, defaultControlBytesPerBlock: Int = 0) {
        val controlBytesPerBlock =
            if (defaultControlBytesPerBlock <= 0) audioSamplesPerBlock * 4
            else defaultControlBytesPerBlock
        runCatchingRemoteException {
            proxy.prepare(audioSamplesPerBlock, defaultControlBytesPerBlock)
            state = InstanceState.INACTIVE
        }
    }

    fun activate() {
        runCatchingRemoteException {
            proxy.activate()

            state = InstanceState.ACTIVE
        }
    }

    fun deactivate() {
        runCatchingRemoteException {
            proxy.deactivate()

            state = InstanceState.INACTIVE
        }
    }

    fun process() {
        runCatchingRemoteException {
            proxy.process(0)
        }
    }

    fun destroy() {
        if (state == InstanceState.ACTIVE)
            deactivate()
        if (state == InstanceState.INACTIVE || state == InstanceState.ERROR) {
            try {
                proxy.destroy()
            } catch (ex: Exception) {
                // nothing we can do here
            }
            /*
            shmList.forEach { shm ->
                shm.shm.close()
            }
            */

            state = InstanceState.DESTROYED
        }
    }

    // port/buffer manipulation
    fun getPortCount() = proxy.getPortCount()
    fun getPort(index: Int) = proxy.getPort(index)
    fun getPortBuffer(portIndex: Int, buffer: ByteBuffer, size: Int) = proxy.getPortBuffer(portIndex, buffer, size)
    fun setPortBuffer(portIndex: Int, buffer: ByteBuffer, size: Int) = proxy.setPortBuffer(portIndex, buffer, size)

    // Standard Extensions

    fun getStateSize() : Int = proxy.getStateSize()
    fun getState(data: ByteArray) = proxy.getState(data)
    fun setState(data: ByteArray) = proxy.setState(data)

    fun getPresetCount() = proxy.getPresetCount()
    fun getCurrentPresetIndex() = proxy.getCurrentPresetIndex()
    fun setCurrentPresetIndex(index: Int) = proxy.setCurrentPresetIndex(index)
    fun getCurrentPresetName(index: Int) = proxy.getCurrentPresetName(index)
}
