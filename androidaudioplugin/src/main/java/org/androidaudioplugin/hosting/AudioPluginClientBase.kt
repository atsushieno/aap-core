package org.androidaudioplugin.hosting

import android.content.Context
import android.media.AudioManager
import org.androidaudioplugin.AudioPluginNatives
import org.androidaudioplugin.PluginInformation

open class AudioPluginClientBase(private val applicationContext: Context) {
    // Service connection
    val serviceConnector = AudioPluginServiceConnector(applicationContext)

    val pluginInstantiatedListeners = mutableListOf<(conn: AudioPluginInstance) -> Unit>()

    val instantiatedPlugins = mutableListOf<AudioPluginInstance>()

    var onInstanceDestroyed: (instance: AudioPluginInstance) -> Unit = {}

    fun dispose() {
        for (instance in instantiatedPlugins)
            instance.destroy()
        serviceConnector.close()
    }

    fun connectToPluginServiceAsync(packageName: String, callback: (PluginServiceConnection?, Exception?) -> Unit) {
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

    @Deprecated("Use safer option of combining connectToPluginServiceAsync and instantiatePlugin",
        replaceWith = ReplaceWith("connectToPluginServiceAsync(pluginInfo.packageName) { _, connectionError -> " +
                " if (connectionError != null) callback(null, connectionError)" +
                " else try { callback(instantiatePlugin(pluginInfo), null) } catch (instancingError: Exception) { callback(null, instancingError) } }"))
    fun instantiatePluginAsync(pluginInfo: PluginInformation, callback: (AudioPluginInstance?, Exception?) -> Unit)
    {
        connectToPluginServiceAsync(pluginInfo.packageName) { conn: PluginServiceConnection?, error: Exception? ->
            if (conn != null) {
                val instance = AudioPluginInstance(serviceConnector, conn,
                    {i ->
                        instantiatedPlugins.remove(i)
                        onInstanceDestroyed(i) }, pluginInfo, sampleRate)
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