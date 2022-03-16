package org.androidaudioplugin.hosting

import android.content.Context
import android.media.AudioManager
import org.androidaudioplugin.AudioPluginExtensionData
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

    // Plugin instancing

    fun instantiatePlugin(pluginInfo: PluginInformation)
    {
        val conn = serviceConnector.findExistingServiceConnection(pluginInfo.packageName, pluginInfo.localName)
        if (conn == null) {
            var serviceConnectedListener: (PluginServiceConnection) -> Unit ={}
            serviceConnectedListener = { c ->
                serviceConnector.serviceConnectedListeners.remove(serviceConnectedListener)
                instantiatePlugin(pluginInfo, c)
            }
            serviceConnector.serviceConnectedListeners.add(serviceConnectedListener)
            val service = AudioPluginHostHelper.queryAudioPluginServices(applicationContext)
                .first { c -> c.plugins.any { p -> p.pluginId == pluginInfo.pluginId }}
            serviceConnector.bindAudioPluginService(service, sampleRate)
        }
        else
            instantiatePlugin(pluginInfo, conn)
    }

    private fun instantiatePlugin(pluginInfo: PluginInformation, conn: PluginServiceConnection)
    {
        val instance = conn.instantiatePlugin(pluginInfo, sampleRate, extensions)
        instantiatedPlugins.add(instance)
        pluginInstantiatedListeners.forEach { l -> l (instance) }
    }

    var sampleRate : Int

    init {
        AudioPluginNatives.initializeAAPJni(applicationContext)

        val audioManager = applicationContext.getSystemService(Context.AUDIO_SERVICE) as AudioManager
        sampleRate = audioManager.getProperty(AudioManager.PROPERTY_OUTPUT_SAMPLE_RATE)?.toInt() ?: 44100
    }
}