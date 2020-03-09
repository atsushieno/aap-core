package org.androidaudioplugin

import android.content.ComponentName
import android.content.ServiceConnection
import android.os.IBinder
import android.util.Log

class AudioPluginServiceConnection(var serviceInfo: AudioPluginServiceInformation, var onConnectedCallback: (conn: AudioPluginServiceConnection) -> Unit) :
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

    fun instantiatePlugin(pluginInfo : PluginInformation, sampleRate: Int) : AudioPluginInstance {

        var instanceId = AudioPluginInterface.Stub.asInterface(binder!!).create(pluginInfo.pluginId, sampleRate)
        var instance = AudioPluginInstance(instanceId, serviceInfo.plugins.first { p -> p.pluginId == pluginInfo.pluginId}, this)
        instances.add(instance)
        return instance
    }
}