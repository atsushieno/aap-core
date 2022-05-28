package org.androidaudioplugin.hosting

import android.content.ComponentName
import android.content.ServiceConnection
import android.os.Handler
import android.os.IBinder
import android.os.Messenger
import android.os.ParcelFileDescriptor
import android.os.SharedMemory
import android.util.Log
import org.androidaudioplugin.AudioPluginExtensionData
import org.androidaudioplugin.PluginInformation
import org.androidaudioplugin.PluginServiceInformation
import org.androidaudioplugin.AudioPluginInterface
import org.androidaudioplugin.AudioPluginInterfaceCallback

class PluginServiceConnection(var serviceInfo: PluginServiceInformation, val binder: IBinder) {

    var instances = mutableListOf<AudioPluginInstance>()

    fun instantiatePlugin(pluginInfo : PluginInformation, sampleRate: Int, extensions: List<AudioPluginExtensionData>) : AudioPluginInstance {
        val instanceId = aapSvc.beginCreate(pluginInfo.pluginId, sampleRate)
        val extensionSharedMemoryList = extensions.associateBy({ ext -> ext.uri}, { ext ->
            val shm = SharedMemory.create(null, ext.data.size)
            shm.mapReadWrite().put(ext.data)
            shm })
        extensionSharedMemoryList.forEach { ext -> aapSvc.addExtension(instanceId, ext.key, ParcelFileDescriptor.fromFd(ext.value.describeContents()), ext.value.size) }
        aapSvc.endCreate(instanceId);
        val instance = AudioPluginInstance(instanceId, serviceInfo.plugins.first { p -> p.pluginId == pluginInfo.pluginId}, this)
        instances.add(instance)
        return instance
    }

    fun setCallback(callback: AudioPluginInterfaceCallback) {
        aapSvc.setCallback(callback)
    }

    private val aapSvc: AudioPluginInterface = AudioPluginInterface.Stub.asInterface(binder)
}
