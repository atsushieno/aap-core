package org.androidaudioplugin.midideviceservice

import android.content.Context
import android.media.AudioManager
import android.os.IBinder
import org.androidaudioplugin.hosting.AudioPluginServiceConnector
import org.androidaudioplugin.PluginServiceInformation
import org.androidaudioplugin.PluginInformation

// Unlike MidiReceiver, it is instantiated whenever the port is opened, and disposed every time it is closed.
// By isolating most of the implementation here, it makes better lifetime management.
class AudioPluginMidiDeviceInstance(private val pluginId: String, private val ownerService: AudioPluginMidiDeviceService) {

    // It is used to manage Service connections, not instancing (which is managed by native code).
    private val serviceConnector: AudioPluginServiceConnector

    private val sampleRate: Int
    private val oboeFrameSize: Int
    private val audioOutChannelCount: Int = 2
    private val aapFrameSize = 512

    private val pendingInstantiationList = mutableListOf<PluginInformation>()

    init {
        val audioManager = ownerService.applicationContext.getSystemService(Context.AUDIO_SERVICE) as AudioManager
        sampleRate = audioManager.getProperty(AudioManager.PROPERTY_OUTPUT_SAMPLE_RATE)?.toInt() ?: 44100
        oboeFrameSize = audioManager.getProperty(AudioManager.PROPERTY_OUTPUT_FRAMES_PER_BUFFER)?.toInt() ?: 1024

        initializeMidiProcessor(ownerService.plugins.toTypedArray(),
            sampleRate, oboeFrameSize, audioOutChannelCount, aapFrameSize)

        serviceConnector = AudioPluginServiceConnector(ownerService.applicationContext)
        serviceConnector.serviceConnectedListeners.add { connection ->
            registerPluginService(
                connection.binder!!,
                connection.serviceInfo.packageName,
                connection.serviceInfo.className
            )

            for (i in pendingInstantiationList)
                if (i.packageName == connection.serviceInfo.packageName && i.localName == connection.serviceInfo.className)
                    instantiatePlugin(i.pluginId!!)

            activate()
        }

        val plugin = ownerService.plugins.first { p -> p.pluginId == pluginId }
        pendingInstantiationList.add(plugin)
        connectService(plugin.packageName, plugin.localName)
    }

    fun onDeviceClosed() {
        deactivate()
        serviceConnector.close()
        terminateMidiProcessor()
    }

    private fun connectService(packageName: String, className: String) {
        if (!serviceConnector.connectedServices.any { s -> s.serviceInfo.packageName == packageName && s.serviceInfo.className == className })
            serviceConnector.bindAudioPluginService(PluginServiceInformation("", packageName, className), sampleRate)
    }

    fun onSend(msg: ByteArray?, offset: Int, count: Int, timestamp: Long) {
        // We skip too lengthy MIDI buffer, dropped at frame size.
        val actualSize = if (count > aapFrameSize) aapFrameSize else count

        processMessage(msg, offset, actualSize, timestamp)
    }

    // Initialize basic native parts, without any plugin information.
    private external fun initializeMidiProcessor(knownPlugins: Array<PluginInformation>, sampleRate: Int, oboeFrameSize: Int, audioOutChannelCount: Int, aapFrameSize: Int)
    private external fun terminateMidiProcessor()
    // register Binder instance to native host
    private external fun registerPluginService(binder: IBinder, packageName: String, className: String)
    private external fun instantiatePlugin(pluginId: String)
    private external fun processMessage(msg: ByteArray?, offset: Int, count: Int, timestampInNanoseconds: Long)
    private external fun activate()
    private external fun deactivate()
}