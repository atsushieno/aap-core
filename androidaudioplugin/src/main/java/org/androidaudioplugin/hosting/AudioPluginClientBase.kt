package org.androidaudioplugin.hosting

import android.content.Context
import android.media.AudioManager
import org.androidaudioplugin.AudioPluginNatives
import org.androidaudioplugin.PluginInformation

open class AudioPluginClientBase(private val applicationContext: Context) {
    // Service connection
    private val serviceConnector by lazy { AudioPluginServiceConnector(applicationContext) }
    private val native by lazy { NativePluginClient.createFromConnection(serviceConnector.serviceConnectionId) }

    val serviceConnectionId by lazy { serviceConnector.serviceConnectionId }

    val instantiatedPlugins by lazy { mutableListOf<AudioPluginInstance>() }

    var onInstanceDestroyed: (instance: AudioPluginInstance) -> Unit = {}

    val onConnectedListeners by lazy { serviceConnector.onConnectedListeners }
    val onDisconnectingListeners by lazy { serviceConnector.onDisconnectingListeners }

    fun dispose() {
        instantiatedPlugins.forEach { it.destroy() }
        serviceConnector.connectedServices.forEach { disconnectPluginService(it.serviceInfo.packageName) }
        native.dispose()
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
        val instance = AudioPluginInstance.create(native,
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
