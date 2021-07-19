package org.androidaudioplugin.midideviceservice

import android.content.Context
import android.media.AudioManager
import android.media.midi.MidiReceiver
import android.os.IBinder
import org.androidaudioplugin.AudioPluginServiceConnector
import org.androidaudioplugin.AudioPluginServiceInformation
import org.androidaudioplugin.PluginInformation

// LAMESPEC: This is a failure point in Android MIDI API.
//  onGetInputPortReceivers() should be called no earlier than onCreate() because onCreate() is
//  the method that is supposed to set up the entire Service itself, using fully supplied
//  Application context. Without Application context, you have no access to assets, package
//  manager including Application meta-data (via ServiceInfo).
//
//  However, Android MidiDeviceService invokes that method **at its constructor**! This means,
//  you cannot use any information above to determine how many ports this service has.
//  This includes access to `midi_device_info.xml` ! Of course we cannot do that either.
//
//  Solution? We have to ask you to give explicit number of input and output ports.
//  This violates the basic design guideline that an abstract member should not be referenced
//  at constructor, but it is inevitable due to this Android MIDI API design flaw.
//
// Since it is instantiated at MidiDeviceService initializer, we cannot run any code that
// depends on Application context until MidiDeviceService.onCreate() is invoked, so
// (1) we have a dedicated `initialize()` method to do that job, so (2) do not anything
// before that happens!
open class AudioPluginMidiReceiver(private val ownerService: AudioPluginMidiDeviceService) : MidiReceiver() {
    companion object {
        init {
            System.loadLibrary("aapmidideviceservice")
        }
    }

    lateinit var pluginId: String

    // It is used to manage Service connections, not instancing (which is managed by native code).
    private lateinit var serviceConnector: AudioPluginServiceConnector

    // We cannot make it lateinit var because it is primitive, and cannot initialize at instantiated
    // time as it must be instantiated at MidiDeviceService instantiation time when ApplicationContext
    // is not assigned yet (as onCreate() was not invoked yet!).
    private var sampleRate: Int? = null
    private var oboeFrameSize: Int? = null
    private var audioOutChannelCount: Int = 2
    private var aapFrameSize = 512

    private var isOpen = false

    fun ensureInitialized(pluginId: String) {
        if (isOpen)
            return
        isOpen = true

        this.pluginId = pluginId
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

            for (i in pendingInstantiationList)
                if (i.packageName == connection.serviceInfo.packageName && i.localName == connection.serviceInfo.className)
                    instantiatePlugin(i.pluginId!!)

            activate()
        }
        initializeReceiverNative(ownerService.applicationContext, ownerService.plugins.toTypedArray(), sampleRate!!, oboeFrameSize!!, audioOutChannelCount, aapFrameSize)

        startPluginSetup()
    }

    fun ensureTerminated() {
        if (!isOpen)
            return
        deactivate()
        terminateReceiverNative()
        // FIXME: this has to be called otherwise an old serviceConnector could raise the event
        //  i.e. event could happen twice (but this call causes crash right now).
        //serviceConnector.close()
        isOpen = false
    }

    private fun connectService(packageName: String, className: String) {
        if (!serviceConnector.connectedServices.any { s -> s.serviceInfo.packageName == packageName && s.serviceInfo.className == className })
            serviceConnector.bindAudioPluginService(
                AudioPluginServiceInformation(
                    "",
                    packageName,
                    className
                ), sampleRate!!)
    }

    private fun startPluginSetup() {
        val plugin = ownerService.plugins.first { p -> p.pluginId == pluginId }
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
    private external fun processMessage(msg: ByteArray?, offset: Int, count: Int, timestampInNanoseconds: Long)
    private external fun activate()
    private external fun deactivate()
}