package org.androidaudioplugin.samples.aaphostsample2

import android.content.Context
import android.media.AudioManager
import org.androidaudioplugin.PluginInformation
import org.androidaudioplugin.PluginServiceInformation
import org.androidaudioplugin.hosting.AudioPluginClientBase
import org.androidaudioplugin.hosting.AudioPluginInstance

/*
    The host engine facade that should function as the facade for the app UI.
 */
class PluginHostEngine private constructor(
    // It is used to manage Service connections, not instancing (which is managed by native code).
                       private val client: AudioPluginClientBase,
                       val availablePluginServices: List<PluginServiceInformation>
) {

    companion object {
        fun create(applicationContext: Context, availablePluginServices: List<PluginServiceInformation>): PluginHostEngine {
            val audioManager = applicationContext.getSystemService(Context.AUDIO_SERVICE) as AudioManager
            val sampleRate = audioManager.getProperty(AudioManager.PROPERTY_OUTPUT_SAMPLE_RATE)?.toInt() ?: 44100
            val oboeFrameSize = audioManager.getProperty(AudioManager.PROPERTY_OUTPUT_FRAMES_PER_BUFFER)?.toInt() ?: 1024

            val client = AudioPluginClientBase(applicationContext)

            val ret = PluginHostEngine(client, availablePluginServices)

            // FIXME: adjust audioOutChannelCount and appFrameSize somewhere?

            ret.initializeEngine(client.serviceConnector.instanceId,
                sampleRate, oboeFrameSize, ret.audioOutChannelCount, ret.aapFrameSize)

            client.pluginInstantiatedListeners.add { instance ->
                ret.instantiatePlugin(instance.pluginInfo.pluginId!!)
                ret.activatePlugin()
            }
            return ret
        }

        init {
            System.loadLibrary("aapsamplehostengine2")
        }
    }

    private val audioOutChannelCount: Int = 2
    private val aapFrameSize = 512

    var instance: AudioPluginInstance? = null

    private var pluginInfo: PluginInformation? = null
    var lastError: Exception? = null

    fun loadPlugin(pluginInfo: PluginInformation, callback: (AudioPluginInstance?, Exception?) ->Unit = { _, _ -> }) {
        client.instantiatePluginAsync(pluginInfo) { instance, error ->
            callback(instance, error)
        }
    }
    /*
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

    fun getPluginInfo(pluginId: String) = availablePluginServices.flatMap { s -> s.plugins }
        .firstOrNull { p -> p.pluginId == pluginId }
    */

    // JNI

    external fun initializeEngine(connectorInstanceId: Int, sampleRate: Int, oboeFrameSize: Int, audioOutChannelCount: Int, aapFrameSize: Int)

    external fun instantiatePlugin(pluginId: String)

    external fun activatePlugin()

    external fun deactivatePlugin()

    external fun terminateEngine()

    fun play() {
        // FIXME: implement
    }
}
