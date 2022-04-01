package org.androidaudioplugin.midideviceservice

import android.content.Context
import android.media.AudioManager
import org.androidaudioplugin.hosting.AudioPluginClient
import org.androidaudioplugin.PluginInformation
import org.androidaudioplugin.hosting.AudioPluginInstance

// Unlike MidiReceiver, it is instantiated whenever the port is opened, and disposed every time it is closed.
// By isolating most of the implementation here, it makes better lifetime management.
class AudioPluginMidiDeviceInstance private constructor(
    // It is used to manage Service connections, not instancing (which is managed by native code).
    private val client: AudioPluginClient) {

    companion object {
        fun createAsync(pluginId: String, ownerService: AudioPluginMidiDeviceService, callback: (AudioPluginMidiDeviceInstance?, Exception?) -> Unit) {
            val audioManager = ownerService.applicationContext.getSystemService(Context.AUDIO_SERVICE) as AudioManager
            val sampleRate = audioManager.getProperty(AudioManager.PROPERTY_OUTPUT_SAMPLE_RATE)?.toInt() ?: 44100
            val oboeFrameSize = audioManager.getProperty(AudioManager.PROPERTY_OUTPUT_FRAMES_PER_BUFFER)?.toInt() ?: 1024

            val client = AudioPluginClient(ownerService.applicationContext)

            val ret = AudioPluginMidiDeviceInstance(client)

            // FIXME: adjust audioOutChannelCount and appFrameSize somewhere?

            ret.initializeMidiProcessor(client.serviceConnector.instanceId,
                sampleRate, oboeFrameSize, ret.audioOutChannelCount, ret.aapFrameSize)

            client.pluginInstantiatedListeners.add { instance ->
                ret.instantiatePlugin(instance.pluginInfo.pluginId!!)
                ret.activate()
            }

            val plugin = ownerService.plugins.first { p -> p.pluginId == pluginId }
            client.instantiatePluginAsync(plugin) { _, error ->
                callback(ret, error)
            }
        }
    }

    private val audioOutChannelCount: Int = 2
    private val aapFrameSize = 512

    fun onDeviceClosed() {
        deactivate()
        client.dispose()
        terminateMidiProcessor()
    }

    fun onSend(msg: ByteArray?, offset: Int, count: Int, timestamp: Long) {
        // We skip too lengthy MIDI buffer, dropped at frame size.
        val actualSize = if (count > aapFrameSize) aapFrameSize else count

        processMessage(msg, offset, actualSize, timestamp)
    }

    // Initialize basic native parts, without any plugin information.
    private external fun initializeMidiProcessor(connectorInstanceId: Int, sampleRate: Int, oboeFrameSize: Int, audioOutChannelCount: Int, aapFrameSize: Int)
    private external fun terminateMidiProcessor()
    private external fun instantiatePlugin(pluginId: String)
    private external fun processMessage(msg: ByteArray?, offset: Int, count: Int, timestampInNanoseconds: Long)
    private external fun activate()
    private external fun deactivate()
}