package org.androidaudioplugin

import android.os.ParcelFileDescriptor
import android.os.SharedMemory
import java.nio.ByteBuffer

class AudioPluginInstance(var instanceId: Int, var pluginInfo: PluginInformation, var service: AudioPluginServiceConnection)
{
    private var shared_memory_list = mutableListOf<SharedMemory>()
    private var shared_memory_fd_list = mutableListOf<Int>()
    private var shared_memory_buffer_list = mutableListOf<ByteBuffer>()

    fun prepare(sampleRate: Int, samplesPerBlock: Int) {
        var proxy = AudioPluginInterface.Stub.asInterface(service.binder!!)
        (0 until pluginInfo.getPortCount()).forEach { i ->
            var shm = SharedMemory.create(null, samplesPerBlock * 4)
            var shmFD = AudioPluginNatives.getSharedMemoryFD(shm)
            var buffer = shm.mapReadWrite()
            shared_memory_list.add(shm)
            shared_memory_fd_list.add(shmFD)
            shared_memory_buffer_list.add(buffer)
            proxy.prepareMemory(instanceId, i, ParcelFileDescriptor.adoptFd(shmFD))
        }
        proxy.prepare(instanceId, samplesPerBlock, pluginInfo.ports.size)
    }

    fun process() {
        var proxy = AudioPluginInterface.Stub.asInterface(service.binder!!)
        proxy.process(instanceId, 0)
    }

    fun destroy() {
        var proxy = AudioPluginInterface.Stub.asInterface(service.binder!!)
        proxy.destroy(instanceId)
        shared_memory_fd_list.forEach { i -> AudioPluginNatives.closeSharedMemoryFD(i) }
        shared_memory_list.forEach { shm -> shm.close() }
    }
}