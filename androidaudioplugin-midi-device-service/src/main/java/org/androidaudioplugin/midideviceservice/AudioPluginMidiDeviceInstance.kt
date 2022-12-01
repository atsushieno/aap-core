package org.androidaudioplugin.midideviceservice

import android.content.Context
import android.media.AudioManager
import org.androidaudioplugin.hosting.AudioPluginClientBase

// Unlike MidiReceiver, it is instantiated whenever the port is opened, and disposed every time it is closed.
// By isolating most of the implementation here, it makes better lifetime management.
class AudioPluginMidiDeviceInstance private constructor(
    // It is used to manage Service connections, not instancing (which is managed by native code).
    private val client: AudioPluginClientBase) {

    companion object {
        fun createAsync(pluginId: String, ownerService: AudioPluginMidiDeviceService, callback: (AudioPluginMidiDeviceInstance?, Exception?) -> Unit) {
            val audioManager = ownerService.applicationContext.getSystemService(Context.AUDIO_SERVICE) as AudioManager
            val sampleRate = audioManager.getProperty(AudioManager.PROPERTY_OUTPUT_SAMPLE_RATE)?.toInt() ?: 44100
            val oboeFrameSize = audioManager.getProperty(AudioManager.PROPERTY_OUTPUT_FRAMES_PER_BUFFER)?.toInt() ?: 1024

            val client = AudioPluginClientBase(ownerService.applicationContext)

            val ret = AudioPluginMidiDeviceInstance(client)

            // FIXME: adjust audioOutChannelCount and appFrameSize somewhere?

            ret.initializeMidiProcessor(client.serviceConnector.instanceId,
                sampleRate, oboeFrameSize, ret.audioOutChannelCount, ret.aapFrameSize, ret.midiBufferSize)

            val pluginInfo = ownerService.plugins.first { p -> p.pluginId == pluginId }
            client.connectToPluginServiceAsync(pluginInfo.packageName) { _, error ->
                if (error != null)
                    callback(null, error)
                try {
                    ret.instantiatePlugin(pluginId)
                    ret.activate()
                    callback(ret, error)
                } catch (e: Exception) {
                    callback(null, e)
                }
            }
        }
    }

    private val audioOutChannelCount: Int = 2
    private val aapFrameSize = 512
    private val midiBufferSize = 4096

    fun onDeviceClosed() {
        deactivate()
        client.dispose()
        terminateMidiProcessor()
    }

    fun onSend(msg: ByteArray?, offset: Int, count: Int, timestamp: Long) {
        // We skip too lengthy MIDI buffer.
        val actualSize = if (count > midiBufferSize) midiBufferSize else count

        processMessage(msg, offset, actualSize, timestamp)
    }

    // Initialize basic native parts, without any plugin information.
    private external fun initializeMidiProcessor(connectorInstanceId: Int, sampleRate: Int, oboeFrameSize: Int, audioOutChannelCount: Int, aapFrameSize: Int, midiBufferSize: Int)
    private external fun terminateMidiProcessor()
    private external fun instantiatePlugin(pluginId: String)
    private external fun processMessage(msg: ByteArray?, offset: Int, count: Int, timestampInNanoseconds: Long)
    private external fun activate()
    private external fun deactivate()
}