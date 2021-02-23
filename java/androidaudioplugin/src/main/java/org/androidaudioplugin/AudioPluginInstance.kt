package org.androidaudioplugin

import android.os.ParcelFileDescriptor
import android.os.SharedMemory
import java.nio.ByteBuffer

class AudioPluginInstance
{
    private class SharedMemoryBuffer (var shm : SharedMemory, var fd : Int, var buffer: ByteBuffer)

    private var shm_list = mutableListOf<SharedMemoryBuffer>()

    val proxy : AudioPluginInterface
    var instanceId: Int
    var pluginInfo: PluginInformation
    var service: PluginServiceConnection

    internal constructor (instanceId: Int, pluginInfo: PluginInformation, service: PluginServiceConnection) {
        this.instanceId = instanceId
        this.pluginInfo = pluginInfo
        this.service = service
        proxy = AudioPluginInterface.Stub.asInterface(service.binder!!)
    }

    fun getPortBuffer(port: Int) = shm_list[port].buffer

    fun prepare(sampleRate: Int, samplesPerBlock: Int) {
        (0 until pluginInfo.getPortCount()).forEach { i ->
            var shm = SharedMemory.create(null, samplesPerBlock * 4)
            var shmFD = AudioPluginNatives.getSharedMemoryFD(shm)
            var buffer = shm.mapReadWrite()
            shm_list.add(SharedMemoryBuffer(shm, shmFD, buffer))
            proxy.prepareMemory(instanceId, i, ParcelFileDescriptor.adoptFd(shmFD))
        }
        proxy.prepare(instanceId, samplesPerBlock, pluginInfo.ports.size)
    }

    fun activate() {
        proxy.activate(instanceId)
    }

    fun process() {
        proxy.process(instanceId, 0)
    }

    fun deactivate() {
        proxy.deactivate(instanceId)
    }

    fun getState() : ByteArray {
        var size = proxy.getStateSize(instanceId)
        var shm = SharedMemory.create(null, size)
        var shmFD = AudioPluginNatives.getSharedMemoryFD(shm)
        proxy.getState(instanceId, ParcelFileDescriptor.adoptFd(shmFD))
        var buffer = shm.mapReadWrite()
        var array = ByteArray(size)
        buffer.get(array, 0, size)
        return array
    }

    fun setState(data: ByteArray) {
        var size = data.size
        var shm = SharedMemory.create(null, size)
        var shmFD = AudioPluginNatives.getSharedMemoryFD(shm)
        var buffer = shm.mapReadWrite()
        buffer.put(buffer)
        proxy.setState(instanceId, ParcelFileDescriptor.adoptFd(shmFD), size)
    }

    fun destroy() {
        proxy.destroy(instanceId)
        shm_list.forEach { shm ->
            shm.shm.close()
        }
    }
}