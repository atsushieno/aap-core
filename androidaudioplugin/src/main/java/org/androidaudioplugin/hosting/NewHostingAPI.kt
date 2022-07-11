package org.androidaudioplugin.hosting

import android.content.Context
import android.os.ParcelFileDescriptor
import android.os.RemoteException
import android.os.SharedMemory
import android.util.Log
import org.androidaudioplugin.AudioPluginExtensionData
import org.androidaudioplugin.AudioPluginNatives
import org.androidaudioplugin.PluginInformation
import org.androidaudioplugin.PortInformation
import java.nio.ByteBuffer
import kotlin.properties.Delegates


/*
  AudioPluginInstance implementation that is based on native `RemotePluginInstance`.
 */
class AudioPluginInstanceNW(
    serviceConnector: AudioPluginServiceConnector,
    conn: PluginServiceConnection,
    override var pluginInfo: PluginInformation,
    sampleRate: Int,
    extensions: List<AudioPluginExtensionData>) : AudioPluginInstance {

    private val client = PluginClient(sampleRate, serviceConnector)
    var state = AudioPluginInstance.InstanceState.UNPREPARED

    private class SharedMemoryBuffer (var shm : SharedMemory, var fd : Int, var buffer: ByteBuffer)
    private var shmList = mutableListOf<SharedMemoryBuffer>()
    private val proxy : RemotePluginInstance

    override var proxyError: Exception? = null

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

    override fun prepare(audioSamplesPerBlock: Int, defaultControlBytesPerBlock: Int) {
        val controlBytesPerBlock =
            if (defaultControlBytesPerBlock <= 0) audioSamplesPerBlock * 4
            else defaultControlBytesPerBlock
        runCatchingRemoteException {
            proxy.beginPrepare(audioSamplesPerBlock, pluginInfo.ports.size)
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
                shmList.add(SharedMemoryBuffer(shm, shmFD, buffer))
                proxy.prepareMemory(i, ParcelFileDescriptor.adoptFd(shmFD))
            }
            proxy.endPrepare()

            state = AudioPluginInstance.InstanceState.INACTIVE
        }
    }

    override fun activate() {
        runCatchingRemoteException {
            proxy.activate()

            state = AudioPluginInstance.InstanceState.ACTIVE
        }
    }

    override fun deactivate() {
        runCatchingRemoteException {
            proxy.deactivate()

            state = AudioPluginInstance.InstanceState.INACTIVE
        }
    }

    override fun process() {
        runCatchingRemoteException {
            proxy.process(0)
        }
    }

    override fun destroy() {
        if (state == AudioPluginInstance.InstanceState.ACTIVE)
            deactivate()
        if (state == AudioPluginInstance.InstanceState.INACTIVE || state == AudioPluginInstance.InstanceState.ERROR) {
            try {
                proxy.destroy()
            } catch (ex: Exception) {
                // nothing we can do here
            }
            shmList.forEach { shm ->
                shm.shm.close()
            }

            state = AudioPluginInstance.InstanceState.DESTROYED
        }
    }

    override fun getPortBuffer(port: Int) = shmList[port].buffer
}


/*
  This class wraps native aap::PluginClient and provides the corresponding features to the native API, to some extent.
 */
class PluginClient(private val sampleRate: Int, val serviceConnector: AudioPluginServiceConnector) {

    // aap::PluginClient*
    val native: Long = newInstance(serviceConnector.instanceId)

    fun createInstanceFromExistingConnection(pluginId: String) : RemotePluginInstance {
        return RemotePluginInstance(pluginId, sampleRate, this)
    }

    companion object {
        @JvmStatic
        private external fun newInstance(connectorInstanceId: Int) : Long
    }
}

/* maps to aap::RemotePluginInstance */
class RemotePluginInstance(val pluginId: String,
                           sampleRate: Int,
                           val client: PluginClient) {
    fun beginPrepare(frameCount: Int, portCount: Int) {
        nativeAudioPluginBuffer = createAudioPluginBuffer(frameCount, portCount)
        beginPrepare(client.native, instanceId, nativeAudioPluginBuffer)
    }
    fun prepareMemory(index: Int, fd: ParcelFileDescriptor) {
        setSharedMemoryToPluginBuffer(nativeAudioPluginBuffer, index, fd.fd)
    }
    fun endPrepare() = endPrepare(client.native, instanceId, nativeAudioPluginBuffer)
    fun activate() = activate(client.native, instanceId)
    fun process(timeoutInNanoseconds: Int) = process(client.native, instanceId, nativeAudioPluginBuffer, timeoutInNanoseconds)
    fun deactivate() = deactivate(client.native, instanceId)
    fun destroy() {
        destroyAudioPluginBuffer(nativeAudioPluginBuffer)
        destroy(client.native, instanceId)
    }

    fun getStateSize() : Int = getStateSize(client.native, instanceId)
    fun getState(data: ByteArray) = getState(client.native, instanceId, data)
    fun setState(data: ByteArray) = setState(client.native, instanceId, data)

    // aap::RemotePluginInstance*
    private val instanceId: Int = createRemotePluginInstance(pluginId, sampleRate, client.native)
    private var nativeAudioPluginBuffer by Delegates.notNull<Long>()

    companion object {
        @JvmStatic
        private external fun createAudioPluginBuffer(frameCount: Int, portCount: Int) : Long
        @JvmStatic
        private external fun destroyAudioPluginBuffer(pointer: Long)
        // Note that it returns an instanceId within the client, not the pointer to the instance.
        // Therefore it returns Int, not Long.
        @JvmStatic
        private external fun createRemotePluginInstance(pluginId: String, sampleRate: Int, nativeClient: Long) : Int
        @JvmStatic
        external fun beginPrepare(nativeClient: Long, instanceId: Int, nativeAudioPluginBuffer: Long)
        @JvmStatic
        external fun endPrepare(nativeClient: Long, instanceId: Int, nativeAudioPluginBuffer: Long)
        @JvmStatic
        external fun activate(nativeClient: Long, instanceId: Int)
        @JvmStatic
        external fun process(nativeClient: Long, instanceId: Int, nativeAudioPluginBuffer: Long, timeoutInNanoseconds: Int)
        @JvmStatic
        external fun deactivate(nativeClient: Long, instanceId: Int)
        @JvmStatic
        external fun destroy(nativeClient: Long, instanceId: Int)
        @JvmStatic
        external fun getStateSize(nativeClient: Long, instanceId: Int) : Int
        @JvmStatic
        external fun getState(nativeClient: Long, instanceId: Int, data: ByteArray)
        @JvmStatic
        external fun setState(nativeClient: Long, instanceId: Int, data: ByteArray)
        @JvmStatic
        external fun setSharedMemoryToPluginBuffer(nativeAudioPluginBuffer: Long, index: Int, fd: Int)
    }
}
