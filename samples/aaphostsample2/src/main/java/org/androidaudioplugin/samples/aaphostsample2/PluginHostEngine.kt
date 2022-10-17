package org.androidaudioplugin.samples.aaphostsample2

import android.content.Context
import org.androidaudioplugin.PluginInformation
import org.androidaudioplugin.PluginServiceInformation
import org.androidaudioplugin.hosting.AudioPluginClient
import org.androidaudioplugin.hosting.AudioPluginInstance

/*
    The host engine facade that should function as the facade for the app UI.
 */
class PluginHostEngine(context: Context,
    val availablePluginServices: List<PluginServiceInformation>) {

    var instance: AudioPluginInstance? = null
    private val host : AudioPluginClient = AudioPluginClient(context.applicationContext)

    private var pluginInfo: PluginInformation? = null
    var lastError: Exception? = null

    fun loadPlugin(pluginInfo: PluginInformation, callback: (AudioPluginInstance?, Exception?) ->Unit = { _, _ -> }) {
        if (instance != null) {
            callback(null, Exception("A plugin ${pluginInfo.pluginId} is already loaded"))
        } else {
            this.pluginInfo = pluginInfo
            host.instantiatePluginAsync(pluginInfo) { instance, error ->
                if (error != null) {
                    lastError = error
                    callback(null, error)
                } else if (instance != null) { // should be always true
                    this.instance = instance
                    instance.prepare(host.audioBufferSizeInBytes / 4, host.defaultControlBufferSizeInBytes)  // 4 is sizeof(float)
                    if (instance.proxyError != null) {
                        callback(null, instance.proxyError!!)
                    } else {
                        callback(instance, null)
                    }
                }
            }
        }
    }

    fun unloadPlugin() {
        val instance = this.instance
        if (instance != null) {
            if (instance.state == AudioPluginInstance.InstanceState.ACTIVE)
                instance.deactivate()
            instance.destroy()
        }

        host.serviceConnector.unbindAudioPluginService(instance!!.pluginInfo.packageName)
        pluginInfo = null
        this.instance = null
    }

    fun getPluginInfo(pluginId: String) = availablePluginServices.flatMap { s -> s.plugins }
        .firstOrNull { p -> p.pluginId == pluginId }

    fun activatePlugin() {
        instance?.activate()
    }

    fun deactivatePlugin() {
        instance?.deactivate()
    }

    fun play() {
        // FIXME: implement
    }
}
