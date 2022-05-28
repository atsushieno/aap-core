package org.androidaudioplugin.hosting

import android.content.Context
import android.media.AudioManager
import org.androidaudioplugin.AudioPluginExtensionData
import org.androidaudioplugin.AudioPluginInterfaceCallback
import org.androidaudioplugin.AudioPluginNatives
import org.androidaudioplugin.PluginInformation

open class AudioPluginClient(private val applicationContext: Context) {
    // Service connection
    val serviceConnector = AudioPluginServiceConnector(applicationContext)

    val pluginInstantiatedListeners = mutableListOf<(conn: AudioPluginInstance) -> Unit>()

    val instantiatedPlugins = mutableListOf<AudioPluginInstance>()

    val extensions = mutableListOf<AudioPluginExtensionData>()

    fun dispose() {
        for (instance in instantiatedPlugins)
            instance.destroy()
        serviceConnector.close()
    }

    private fun withPluginConnection(packageName: String, callback: (PluginServiceConnection?, Exception?) -> Unit) {
        val conn = serviceConnector.findExistingServiceConnection(packageName)
        if (conn == null) {
            var serviceConnectedListener: (PluginServiceConnection?, Exception?) -> Unit = { _, _ -> }
            serviceConnectedListener = { c, e ->
                serviceConnector.serviceConnectedListeners.remove(serviceConnectedListener)
                callback(c, e)
            }
            serviceConnector.serviceConnectedListeners.add(serviceConnectedListener)
            val service = AudioPluginHostHelper.queryAudioPluginServices(applicationContext)
                .first { c -> c.packageName == packageName }
            serviceConnector.bindAudioPluginService(service)
        }
        else
            callback(conn, null)

    }

    fun setCallback(pluginInfo: PluginInformation, callback: AudioPluginInterfaceCallback, onError: (Exception) -> Unit) {
        withPluginConnection(pluginInfo.packageName) { conn: PluginServiceConnection?, error: Exception? ->
            if (conn != null)
                conn.setCallback(callback)
            else
                onError(error!!)
        }
    }

    // Plugin instancing

    fun instantiatePluginAsync(pluginInfo: PluginInformation, callback: (AudioPluginInstance?, Exception?) -> Unit)
    {
        withPluginConnection(pluginInfo.packageName) { conn: PluginServiceConnection?, error: Exception? ->
            if (conn != null) {
                val instance = conn.instantiatePlugin(pluginInfo, sampleRate, extensions)
                instantiatedPlugins.add(instance)
                pluginInstantiatedListeners.forEach { l -> l(instance) }
                callback(instance, null)
            }
            else
                callback(null, error)
        }
    }

    var sampleRate : Int

    init {
        AudioPluginNatives.initializeAAPJni(applicationContext)

        val audioManager = applicationContext.getSystemService(Context.AUDIO_SERVICE) as AudioManager
        sampleRate = audioManager.getProperty(AudioManager.PROPERTY_OUTPUT_SAMPLE_RATE)?.toInt() ?: 44100
    }
}