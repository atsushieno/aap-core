package org.androidaudioplugin.hosting

import android.content.Context
import android.media.AudioManager
import org.androidaudioplugin.AudioPluginNatives
import org.androidaudioplugin.PluginInformation

open class AudioPluginClientBase(private val applicationContext: Context) {
    // Service connection
    val serviceConnector = AudioPluginServiceConnector(applicationContext)

    private var native : NativePluginClient? = null

    val instantiatedPlugins = mutableListOf<AudioPluginInstance>()

    var onInstanceDestroyed: (instance: AudioPluginInstance) -> Unit = {}

    fun dispose() {
        instantiatedPlugins.forEach { it.destroy() }
        serviceConnector.connectedServices.forEach { disconnectPluginService(it.serviceInfo.packageName) }
        native?.dispose()
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

    fun disconnectPluginService(packageName: String) {
        val conn = serviceConnector.findExistingServiceConnection(packageName)
        if (conn != null)
            serviceConnector.unbindAudioPluginService(conn.serviceInfo.packageName)
    }

    fun instantiatePlugin(pluginInfo: PluginInformation) : AudioPluginInstance {
        val conn = serviceConnector.findExistingServiceConnection(pluginInfo.packageName)
        assert(conn != null)
        if (native == null)
            native = NativePluginClient.createFromConnection(serviceConnector.serviceConnectionId)
        val instance = AudioPluginInstance.create(serviceConnector, conn!!, native!!,
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
