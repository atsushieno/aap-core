package org.androidaudioplugin.hosting

import android.content.Context
import android.media.AudioManager
import org.androidaudioplugin.AudioPluginNatives
import org.androidaudioplugin.PluginInformation

open class AudioPluginClientBase(private val context: Context) {
    // Service connection
    protected val serviceConnector by lazy { AudioPluginServiceConnector(context) }
    protected val native by lazy { NativePluginClient.createFromConnection(serviceConnector.serviceConnectionId) }

    val serviceConnectionId by lazy { serviceConnector.serviceConnectionId }

    val onConnectedListeners by lazy { serviceConnector.onConnectedListeners }
    val onDisconnectingListeners by lazy { serviceConnector.onDisconnectingListeners }

    fun dispose() {
        onDispose()
        // disconnecting removes the connection from connectedServices, so iterate over a copy.
        serviceConnector.connectedServices.toList().forEach {
            disconnectPluginService(it.serviceInfo.packageName, it.serviceInfo.className)
        }
        native.dispose()
    }

    open fun onDispose() {}

    /**
     * Connects to the primary AudioPluginService of [packageName]
     * (see [AudioPluginHostHelper.selectPrimaryAudioPluginService]). A plugin package may have more
     * than one AudioPluginService, in separate processes, and the plugins of the other services are
     * not reachable through this connection.
     */
    @Deprecated("A plugin package may have more than one AudioPluginService. Use the overload that takes the service class name (PluginInformation.localName).",
        ReplaceWith("connectToPluginService(packageName, className)"))
    suspend fun connectToPluginService(packageName: String) : PluginServiceConnection {
        val service = AudioPluginHostHelper.queryPrimaryAudioPluginService(context.applicationContext, packageName)
        return connectToPluginService(service.packageName, service.className)
    }

    /**
     * Connects to the AudioPluginService [packageName]/[className]. For a plugin, they are
     * [PluginInformation.packageName] and [PluginInformation.localName].
     */
    suspend fun connectToPluginService(packageName: String, className: String) : PluginServiceConnection =
        serviceConnector.findExistingServiceConnection(packageName, className)
            ?: serviceConnector.bindAudioPluginService(
                AudioPluginHostHelper.queryAudioPluginService(context.applicationContext, packageName, className))

    /** Connects to the AudioPluginService that hosts [pluginInfo]. */
    suspend fun connectToPluginService(pluginInfo: PluginInformation) : PluginServiceConnection =
        connectToPluginService(pluginInfo.packageName, pluginInfo.localName)

    @Deprecated("A plugin package may have more than one AudioPluginService. Use the overload that takes the service class name (PluginInformation.localName).",
        ReplaceWith("disconnectPluginService(packageName, className)"))
    fun disconnectPluginService(packageName: String) {
        val conn = serviceConnector.findExistingServiceConnection(packageName)
        if (conn != null)
            serviceConnector.unbindAudioPluginService(conn.serviceInfo.packageName, conn.serviceInfo.className)
    }

    fun disconnectPluginService(packageName: String, className: String) {
        serviceConnector.unbindAudioPluginService(packageName, className)
    }

    fun instantiateNativePlugin(pluginInfo: PluginInformation) : NativeRemotePluginInstance {
        val conn = serviceConnector.findExistingServiceConnection(pluginInfo.packageName, pluginInfo.localName)
        assert(conn != null)
        return native.createInstanceFromExistingConnection(pluginInfo.pluginId!!)
    }

    var sampleRate : Int

    init {
        AudioPluginNatives.initializeAAPJni(context.applicationContext)

        val audioManager = context.getSystemService(Context.AUDIO_SERVICE) as AudioManager
        sampleRate = audioManager.getProperty(AudioManager.PROPERTY_OUTPUT_SAMPLE_RATE)?.toInt() ?: 48000
    }
}
