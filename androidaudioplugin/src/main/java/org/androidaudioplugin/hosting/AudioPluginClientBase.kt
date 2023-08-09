package org.androidaudioplugin.hosting

import android.content.Context
import android.media.AudioManager
import org.androidaudioplugin.AudioPluginNatives
import org.androidaudioplugin.PluginInformation

open class AudioPluginClientBase(private val applicationContext: Context) {
    // Service connection
    protected val serviceConnector by lazy { AudioPluginServiceConnector(applicationContext) }
    protected val native by lazy { NativePluginClient.createFromConnection(serviceConnector.serviceConnectionId) }

    val serviceConnectionId by lazy { serviceConnector.serviceConnectionId }

    val onConnectedListeners by lazy { serviceConnector.onConnectedListeners }
    val onDisconnectingListeners by lazy { serviceConnector.onDisconnectingListeners }

    fun dispose() {
        onDispose()
        serviceConnector.connectedServices.forEach { disconnectPluginService(it.serviceInfo.packageName) }
        native.dispose()
    }

    open fun onDispose() {}

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

    fun instantiateNativePlugin(pluginInfo: PluginInformation) : NativeRemotePluginInstance {
        val conn = serviceConnector.findExistingServiceConnection(pluginInfo.packageName)
        assert(conn != null)
        return native.createInstanceFromExistingConnection(sampleRate, pluginInfo.pluginId!!)
    }

    var sampleRate : Int

    init {
        AudioPluginNatives.initializeAAPJni(applicationContext)

        val audioManager = applicationContext.getSystemService(Context.AUDIO_SERVICE) as AudioManager
        sampleRate = audioManager.getProperty(AudioManager.PROPERTY_OUTPUT_SAMPLE_RATE)?.toInt() ?: 44100
    }
}
