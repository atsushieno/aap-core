package org.androidaudioplugin.midideviceservice

import android.media.midi.MidiDeviceService
import android.media.midi.MidiDeviceStatus
import android.media.midi.MidiReceiver
import org.androidaudioplugin.*

abstract class AudioPluginMidiDeviceService : MidiDeviceService() {

    abstract fun getPluginId(portIntex: Int): String

    open val plugins: List<PluginInformation>
        get() = AudioPluginHostHelper.getLocalAudioPluginService(applicationContext).plugins
            .filter { p -> isInstrument(p) }

    private fun isInstrument(info: PluginInformation) =
        info.category?.contains(PluginInformation.PRIMARY_CATEGORY_INSTRUMENT) ?: info.category?.contains("Synth") ?: false

    // it does not really do any work but initializing native PAL.
    private lateinit var host: AudioPluginHost

    override fun onCreate() {
        super.onCreate()

        host = AudioPluginHost(applicationContext)
    }

    private var receivers: Array<MidiReceiver>? = null

    override fun onGetInputPortReceivers(): Array<MidiReceiver> {
        receivers = receivers ?: Array(deviceInfo.inputPortCount) { AudioPluginMidiReceiver(this) }
        return receivers!!
    }

    override fun onDeviceStatusChanged(status: MidiDeviceStatus) {
        super.onDeviceStatusChanged(status)

        for (i in 0 until deviceInfo.inputPortCount) {
            if (status.isInputPortOpen(i))
                (receivers!![i] as AudioPluginMidiReceiver).ensureInitialized(getPluginId(i))
            else
                (receivers!![i] as AudioPluginMidiReceiver).ensureTerminated()
        }
    }
}



