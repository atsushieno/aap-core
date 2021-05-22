package org.androidaudioplugin.midideviceservice

import android.content.ComponentName
import android.content.Context
import android.content.pm.PackageManager
import android.content.pm.ServiceInfo
import android.media.AudioManager
import android.media.midi.MidiDeviceService
import android.media.midi.MidiDeviceStatus
import android.media.midi.MidiReceiver
import android.os.IBinder
import org.androidaudioplugin.*

private const val MIDI1_PROTOCOL_TYPE = 1
private const val MIDI2_PROTOCOL_TYPE = 2

open class AudioPluginMidiDeviceService : MidiDeviceService() {

    open val plugins: List<PluginInformation>
        get() = AudioPluginHostHelper.getLocalAudioPluginService(applicationContext).plugins

    private fun isInstrument(info: PluginInformation) =
        info.category?.contains(PluginInformation.PRIMARY_CATEGORY_INSTRUMENT) ?: info.category?.contains("Synth") ?: false

    open val instrument: PluginInformation
        get() = plugins.first { p -> isInstrument(p) }

    // The number of ports is not simply adjustable in one code. device_info.xml needs updates too.
    private var midiReceivers: Array<AudioPluginMidiReceiver> = arrayOf(AudioPluginMidiReceiver(this))

    // it does not really do any work but initializing native PAL.
    private lateinit var host: AudioPluginHost

    override fun onCreate() {
        super.onCreate()

        host = AudioPluginHost(applicationContext)
    }

    override fun onGetInputPortReceivers(): Array<MidiReceiver> = midiReceivers.map { e -> e as MidiReceiver }.toTypedArray()

    override fun onDeviceStatusChanged(status: MidiDeviceStatus?) {
        super.onDeviceStatusChanged(status)

        if (status == null) return
        if (status.isInputPortOpen(0))
            midiReceivers[0].initialize()
        else if (!status.isInputPortOpen(0))
            midiReceivers[0].close()
    }
}

class AudioPluginMidiReceiver(private val ownerService: AudioPluginMidiDeviceService) : MidiReceiver(), AutoCloseable {
    companion object {
        init {
            System.loadLibrary("aapmidideviceservice")
        }
    }

    // It is used to manage Service connections, not instancing (which is managed by native code).
    private lateinit var serviceConnector: AudioPluginServiceConnector

    // We cannot make it lateinit var because it is primitive, and cannot initialize at instantiated
    // time as it must be instantiated at MidiDeviceService instantiation time when ApplicationContext
    // is not assigned yet (as onCreate() was not invoked yet!).
    private var sampleRate: Int? = null
    private var oboeFrameSize: Int? = null
    private var audioOutChannelCount: Int = 2
    private var aapFrameSize = 512

    fun initialize() {
        val audioManager = ownerService.getSystemService(Context.AUDIO_SERVICE) as AudioManager
        sampleRate = audioManager.getProperty(AudioManager.PROPERTY_OUTPUT_SAMPLE_RATE)?.toInt() ?: 44100
        oboeFrameSize = (audioManager.getProperty(AudioManager.PROPERTY_OUTPUT_FRAMES_PER_BUFFER)?.toInt() ?: 1024)

        serviceConnector = AudioPluginServiceConnector(ownerService.applicationContext)

        serviceConnector.serviceConnectedListeners.add { connection ->
            registerPluginService(
                connection.binder!!,
                connection.serviceInfo.packageName,
                connection.serviceInfo.className
            )

            if (ownerService.instrument.ports.any { p -> p.content == PortInformation.PORT_CONTENT_TYPE_MIDI2 && p.direction == PortInformation.PORT_DIRECTION_INPUT })
                setMidiProtocol(MIDI2_PROTOCOL_TYPE)

            for (i in pendingInstantiationList)
                if (i.packageName == connection.serviceInfo.packageName && i.localName == connection.serviceInfo.className)
                    instantiatePlugin(i.pluginId!!)

            activate()
        }
        initializeReceiverNative(ownerService.applicationContext, ownerService.plugins.toTypedArray(), sampleRate!!, oboeFrameSize!!, audioOutChannelCount, aapFrameSize)

        setupDefaultPlugins()
    }

    private var closed = false
    override fun close() {
        if (!closed) {
            deactivate()
            terminateReceiverNative()
            // FIXME: this has to be called otherwise an old serviceConnector could raise the event
            //  i.e. event could happen twice (but this call causes crash right now).
            //serviceConnector.close()
            closed = true
        }
    }

    private fun connectService(packageName: String, className: String) {
        if (!serviceConnector.connectedServices.any { s -> s.serviceInfo.packageName == packageName && s.serviceInfo.className == className })
            serviceConnector.bindAudioPluginService(AudioPluginServiceInformation("", packageName, className), sampleRate!!)
    }

    private fun setupDefaultPlugins() {
        val plugin = ownerService.instrument
        if (plugin != null) {
            pendingInstantiationList.add(plugin)
            connectService(plugin.packageName, plugin.localName)
        }
    }

    private val pendingInstantiationList = mutableListOf<PluginInformation>()

    override fun onSend(msg: ByteArray?, offset: Int, count: Int, timestamp: Long) {
        // We skip too lengthy MIDI buffer, dropped at frame size.
        val actualSize = if (count > aapFrameSize) aapFrameSize else count

        processMessage(msg, offset, actualSize, timestamp)
    }

    // Initialize basic native parts, without any plugin information.
    private external fun initializeReceiverNative(applicationContext: Context, knownPlugins: Array<PluginInformation>, sampleRate: Int, oboeFrameSize: Int, audioOutChannelCount: Int, aapFrameSize: Int)
    private external fun terminateReceiverNative()
    // register Binder instance to native host
    private external fun registerPluginService(binder: IBinder, packageName: String, className: String)
    private external fun instantiatePlugin(pluginId: String)
    private external fun setMidiProtocol(midiProtocol: Int)
    private external fun processMessage(msg: ByteArray?, offset: Int, count: Int, timestampInNanoseconds: Long)
    private external fun activate()
    private external fun deactivate()
}
