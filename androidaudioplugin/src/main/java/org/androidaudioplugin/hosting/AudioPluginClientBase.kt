package org.androidaudioplugin.hosting

import android.content.Context
import android.media.AudioManager
import org.androidaudioplugin.AudioPluginNatives
import org.androidaudioplugin.PluginInformation

open class AudioPluginClientBase(private val applicationContext: Context) {
    // Service connection
    val serviceConnector = AudioPluginServiceConnector(applicationContext)

    val instantiatedPlugins = mutableListOf<AudioPluginInstance>()

    var onInstanceDestroyed: (instance: AudioPluginInstance) -> Unit = {}

    fun dispose() {
        for (instance in instantiatedPlugins)
            instance.destroy()
        serviceConnector.close()
    }

    suspend fun connectToPluginService(packageName: String) : PluginServiceConnection {
        val conn = serviceConnector.findExistingServiceConnection(packageName)
        if (conn == null) {
            val service = AudioPluginHostHelper.queryAudioPluginServices(applicationContext)
                .first { c -> c.packageName == packageName }
            return serviceConnector.bindAudioPluginService(service)
        }
        return conn
    }

    fun instantiatePlugin(pluginInfo: PluginInformation) : AudioPluginInstance {
        val conn = serviceConnector.findExistingServiceConnection(pluginInfo.packageName)
        assert(conn != null)
        val instance = AudioPluginInstance(serviceConnector, conn!!,
            {i ->
                instantiatedPlugins.remove(i)
                onInstanceDestroyed(i)
            }, pluginInfo, sampleRate)
        instantiatedPlugins.add(instance)
        return instance
    }

    var sampleRate : Int

    init {
        AudioPluginNatives.initializeAAPJni(applicationContext)

        val audioManager = applicationContext.getSystemService(Context.AUDIO_SERVICE) as AudioManager
        sampleRate = audioManager.getProperty(AudioManager.PROPERTY_OUTPUT_SAMPLE_RATE)?.toInt() ?: 44100
    }
}
