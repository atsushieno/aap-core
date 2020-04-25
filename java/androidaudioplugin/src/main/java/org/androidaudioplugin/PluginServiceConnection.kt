package org.androidaudioplugin

import android.content.ComponentName
import android.content.ServiceConnection
import android.os.IBinder
import android.os.ParcelFileDescriptor
import android.os.SharedMemory
import android.util.Log

// FIXME: make it internal
class PluginServiceConnection(var serviceInfo: AudioPluginServiceInformation, var onConnectedCallback: (conn: PluginServiceConnection) -> Unit) :
    ServiceConnection {

    var binder: IBinder? = null

    var instances = mutableListOf<AudioPluginInstance>()

    override fun onServiceConnected(name: ComponentName?, binder: IBinder?) {
        Log.d("AudioPluginServiceConnection", "onServiceConnected")
        this.binder = binder
        onConnectedCallback(this)
    }

    override fun onServiceDisconnected(name: ComponentName?) {
    }

    fun instantiatePlugin(pluginInfo : PluginInformation, sampleRate: Int, extensions: List<AudioPluginExtensionData>) : AudioPluginInstance {
        val aapSvc = AudioPluginInterface.Stub.asInterface(binder!!)
        var instanceId = aapSvc.create(pluginInfo.pluginId, sampleRate)
        var extensionSharedMemoryList = extensions.associateBy({ ext -> ext.uri}, { ext ->
            val shm = SharedMemory.create(null, ext.data.size)
            shm.mapReadWrite().put(ext.data)
            shm })
        extensionSharedMemoryList.forEach { ext -> aapSvc.addExtension(instanceId, ext.key, ParcelFileDescriptor.fromFd(ext.value.describeContents()), ext.value.size) }
        var instance = AudioPluginInstance(instanceId, serviceInfo.plugins.first { p -> p.pluginId == pluginInfo.pluginId}, this)
        instances.add(instance)
        return instance
    }
}