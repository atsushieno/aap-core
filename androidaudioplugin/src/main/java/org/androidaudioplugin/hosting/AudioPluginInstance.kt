package org.androidaudioplugin.hosting

import android.os.ParcelFileDescriptor
import android.os.RemoteException
import android.os.SharedMemory
import android.util.Log
import org.androidaudioplugin.AudioPluginNatives
import org.androidaudioplugin.PluginInformation
import org.androidaudioplugin.AudioPluginInterface
import org.androidaudioplugin.PortInformation
import java.nio.ByteBuffer

class AudioPluginInstance
{
    enum class InstanceState {
        UNPREPARED,
        INACTIVE,
        ACTIVE,
        DESTROYED,
        ERROR
    }

    private class SharedMemoryBuffer (var shm : SharedMemory, var fd : Int, var buffer: ByteBuffer)

    private var shm_list = mutableListOf<SharedMemoryBuffer>()

    val proxy : AudioPluginInterface
    var instanceId: Int
    var pluginInfo: PluginInformation
    var service: PluginServiceConnection
    var state = InstanceState.UNPREPARED
    var proxyError : Exception? = null

    internal constructor (instanceId: Int, pluginInfo: PluginInformation, service: PluginServiceConnection) {
        this.instanceId = instanceId
        this.pluginInfo = pluginInfo
        this.service = service
        proxy = AudioPluginInterface.Stub.asInterface(service.binder!!)
    }

    fun getPortBuffer(port: Int) = shm_list[port].buffer

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

    private fun runCatchingRemoteException(func: () -> Unit) {
        runCatchingRemoteExceptionFor(Unit) {
            func()
        }
    }

    fun prepare(audioSamplesPerBlock: Int, defaultControlBytesPerBlock: Int = 0) {
        val controlBytesPerBlock =
            if (defaultControlBytesPerBlock <= 0) audioSamplesPerBlock * 4
            else defaultControlBytesPerBlock
        runCatchingRemoteException {
            (0 until pluginInfo.getPortCount()).forEach { i ->
                val port = pluginInfo.getPort(i)
                val isAudio = port.content == PortInformation.PORT_CONTENT_TYPE_AUDIO
                val size =
                    if (port.minimumSizeInBytes > 0) port.minimumSizeInBytes
                    else if (isAudio) audioSamplesPerBlock * 4
                    else controlBytesPerBlock
                val shm = SharedMemory.create(null, size)
                val shmFD = AudioPluginNatives.getSharedMemoryFD(shm)
                val buffer = shm.mapReadWrite()
                shm_list.add(SharedMemoryBuffer(shm, shmFD, buffer))
                proxy.prepareMemory(instanceId, i, ParcelFileDescriptor.adoptFd(shmFD))
            }
            proxy.prepare(instanceId, audioSamplesPerBlock, pluginInfo.ports.size)

            state = InstanceState.INACTIVE
        }
    }

    fun activate() {
        runCatchingRemoteException {
            proxy.activate(instanceId)

            state = InstanceState.ACTIVE
        }
    }

    fun process() {
        runCatchingRemoteException {
            proxy.process(instanceId, 0)
        }
    }

    fun deactivate() {
        runCatchingRemoteException {
            proxy.deactivate(instanceId)

            state = InstanceState.INACTIVE
        }
    }

    fun getState() : ByteArray {
        return runCatchingRemoteExceptionFor(byteArrayOf()) {
            val size = proxy.getStateSize(instanceId)
            val shm = SharedMemory.create(null, size)
            val shmFD = AudioPluginNatives.getSharedMemoryFD(shm)
            proxy.getState(instanceId, ParcelFileDescriptor.adoptFd(shmFD))
            val buffer = shm.mapReadWrite()
            val array = ByteArray(size)
            buffer.get(array, 0, size)
            array
        }
    }

    fun setState(data: ByteArray) {
        runCatchingRemoteException {
            val size = data.size
            val shm = SharedMemory.create(null, size)
            val shmFD = AudioPluginNatives.getSharedMemoryFD(shm)
            val buffer = shm.mapReadWrite()
            buffer.put(buffer)
            proxy.setState(instanceId, ParcelFileDescriptor.adoptFd(shmFD), size)
        }
    }

    fun destroy() {
        if (state == InstanceState.ACTIVE)
            deactivate()
        if (state == InstanceState.INACTIVE || state == InstanceState.ERROR) {
            try {
                proxy.destroy(instanceId)
            } catch (ex: Exception) {
                // nothing we can do here
            }
            shm_list.forEach { shm ->
                shm.shm.close()
            }

            state = InstanceState.DESTROYED
        }
    }
}