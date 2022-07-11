package org.androidaudioplugin.hosting

import android.content.ServiceConnection
import android.os.IBinder
import org.androidaudioplugin.AudioPluginExtensionData
import org.androidaudioplugin.PluginInformation
import org.androidaudioplugin.PluginServiceInformation

class PluginServiceConnection(private val owner: AudioPluginServiceConnector, val platformServiceConnection: ServiceConnection,  val serviceInfo: PluginServiceInformation, val binder: IBinder) {

    val instances = mutableListOf<AudioPluginInstance>()

    fun instantiatePlugin(pluginInfo : PluginInformation, sampleRate: Int, extensions: List<AudioPluginExtensionData>) : AudioPluginInstance {
        return AudioPluginInstance(owner, this, pluginInfo, sampleRate, extensions)
    }

    /*
    fun setCallback(callback: AudioPluginInterfaceCallback) {
        TODO() // FIXME: implement
    }

    fun instantiatePlugin(pluginInfo : PluginInformation, sampleRate: Int, extensions: List<AudioPluginExtensionData>) : AudioPluginInstance {
        val instanceId = aapSvc.beginCreate(pluginInfo.pluginId, sampleRate)
        val extensionSharedMemoryList = extensions.associateBy({ ext -> ext.uri}, { ext ->
            val shm = SharedMemory.create(null, ext.data.size)
            shm.mapReadWrite().put(ext.data)
            shm })
        extensionSharedMemoryList.forEach { ext -> aapSvc.addExtension(instanceId, ext.key, ParcelFileDescriptor.fromFd(ext.value.describeContents()), ext.value.size) }
        aapSvc.endCreate(instanceId)
        val pluginInfo = serviceInfo.plugins.first { p -> p.pluginId == pluginInfo.pluginId}
        val instance = AudioPluginInstanceImpl(instanceId, pluginInfo, this)
        instances.add(instance)
        return instance
        return AudioPluginInnstanceImpl2(applicationContext, pluginInfo, this)
    }

    fun setCallback(callback: AudioPluginInterfaceCallback) {
        aapSvc.setCallback(callback)
    }

    private val aapSvc: AudioPluginInterface = AudioPluginInterface.Stub.asInterface(binder)
    */
}
