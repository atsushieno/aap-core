package org.androidaudioplugin.hosting

import android.os.ParcelFileDescriptor
import android.os.RemoteException
import android.os.SharedMemory
import android.util.Log
import org.androidaudioplugin.*
import org.androidaudioplugin.AudioPluginNatives
import java.nio.ByteBuffer

interface AudioPluginInstance {
    enum class InstanceState {
        UNPREPARED,
        INACTIVE,
        ACTIVE,
        DESTROYED,
        ERROR
    }

    fun prepare(audioSamplesPerBlock: Int, defaultControlBytesPerBlock: Int = 0)
    fun activate()
    fun deactivate()
    fun process()
    fun destroy()

    fun getPortBuffer(port: Int): ByteBuffer

    var pluginInfo: PluginInformation
    var proxyError : Exception?
}

@Deprecated("It is deprecated and removed soon; it will not work once we fix issue #112")
class AudioPluginInstanceImpl : AudioPluginInstance
{
    private class SharedMemoryBuffer (var shm : SharedMemory, var fd : Int, var buffer: ByteBuffer)

    private var shm_list = mutableListOf<SharedMemoryBuffer>()

    val proxy : AudioPluginInterface
    var instanceId: Int
    override var pluginInfo: PluginInformation
    var state = AudioPluginInstance.InstanceState.UNPREPARED
    override var proxyError : Exception? = null

    internal constructor (instanceId: Int, pluginInfo: PluginInformation, service: PluginServiceConnection) {
        this.instanceId = instanceId
        this.pluginInfo = pluginInfo
        proxy = AudioPluginInterface.Stub.asInterface(service.binder!!)
    }

    override fun getPortBuffer(port: Int) = shm_list[port].buffer

    private fun <T> runCatchingRemoteExceptionFor(default: T, func: () -> T) : T {
        if (state == AudioPluginInstance.InstanceState.ERROR)
            return default
        return try {
            func()
        } catch (ex: RemoteException) {
            state = AudioPluginInstance.InstanceState.ERROR
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

    override fun prepare(audioSamplesPerBlock: Int, defaultControlBytesPerBlock: Int) {
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

            state = AudioPluginInstance.InstanceState.INACTIVE
        }
    }

    override fun activate() {
        runCatchingRemoteException {
            proxy.activate(instanceId)

            state = AudioPluginInstance.InstanceState.ACTIVE
        }
    }

    override fun process() {
        runCatchingRemoteException {
            proxy.process(instanceId, 0)
        }
    }

    override fun deactivate() {
        runCatchingRemoteException {
            proxy.deactivate(instanceId)

            state = AudioPluginInstance.InstanceState.INACTIVE
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

    override fun destroy() {
        if (state == AudioPluginInstance.InstanceState.ACTIVE)
            deactivate()
        if (state == AudioPluginInstance.InstanceState.INACTIVE || state == AudioPluginInstance.InstanceState.ERROR) {
            try {
                proxy.destroy(instanceId)
            } catch (ex: Exception) {
                // nothing we can do here
            }
            shm_list.forEach { shm ->
                shm.shm.close()
            }

            state = AudioPluginInstance.InstanceState.DESTROYED
        }
    }
}
