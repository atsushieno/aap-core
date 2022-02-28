package org.androidaudioplugin.midideviceservice

import android.content.Context
import android.media.AudioManager
import android.os.IBinder
import org.androidaudioplugin.AudioPluginServiceClient
import org.androidaudioplugin.hosting.AudioPluginServiceConnector
import org.androidaudioplugin.PluginServiceInformation
import org.androidaudioplugin.PluginInformation

// Unlike MidiReceiver, it is instantiated whenever the port is opened, and disposed every time it is closed.
// By isolating most of the implementation here, it makes better lifetime management.
class AudioPluginMidiDeviceInstance(private val pluginId: String, private val ownerService: AudioPluginMidiDeviceService) {

    // It is used to manage Service connections, not instancing (which is managed by native code).
    private val client: AudioPluginServiceClient

    private val sampleRate: Int
    private val oboeFrameSize: Int
    private val audioOutChannelCount: Int = 2
    private val aapFrameSize = 512

    private val pendingInstantiationList = mutableListOf<PluginInformation>()

    init {
        val audioManager = ownerService.applicationContext.getSystemService(Context.AUDIO_SERVICE) as AudioManager
        sampleRate = audioManager.getProperty(AudioManager.PROPERTY_OUTPUT_SAMPLE_RATE)?.toInt() ?: 44100
        oboeFrameSize = audioManager.getProperty(AudioManager.PROPERTY_OUTPUT_FRAMES_PER_BUFFER)?.toInt() ?: 1024

        client = AudioPluginServiceClient(ownerService.applicationContext)

        initializeMidiProcessor(ownerService.host.serviceConnector,
            sampleRate, oboeFrameSize, audioOutChannelCount, aapFrameSize)

        client.pluginInstantiatedListeners.add { instance ->
            instantiatePlugin(instance.pluginInfo.pluginId!!)
            activate()
        }

        val plugin = ownerService.plugins.first { p -> p.pluginId == pluginId }
        client.instantiatePlugin(plugin)
    }

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
    private external fun initializeMidiProcessor(knownPlugins: AudioPluginServiceConnector, sampleRate: Int, oboeFrameSize: Int, audioOutChannelCount: Int, aapFrameSize: Int)
    private external fun terminateMidiProcessor()
    private external fun instantiatePlugin(pluginId: String)
    private external fun processMessage(msg: ByteArray?, offset: Int, count: Int, timestampInNanoseconds: Long)
    private external fun activate()
    private external fun deactivate()
}