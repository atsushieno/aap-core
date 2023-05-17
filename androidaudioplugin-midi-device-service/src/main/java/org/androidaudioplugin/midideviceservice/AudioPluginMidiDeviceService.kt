package org.androidaudioplugin.midideviceservice

import android.media.midi.MidiDeviceInfo
import android.media.midi.MidiDeviceService
import android.media.midi.MidiDeviceStatus
import android.media.midi.MidiReceiver
import org.androidaudioplugin.*
import org.androidaudioplugin.hosting.AudioPluginHostHelper

abstract class AudioPluginMidiDeviceService : MidiDeviceService() {

    // There is no logical mappings between MIDI device name in "midi_device_info.xml" (or whatever
    // for the metadata) and the plugin display name.
    // This default implementation performs simple and sloppy matching for device name and port name
    // (startsWith() and endsWith()).
    // Our normative use case is aap-lv2-mda which contains multiple plugins within a service.
    open fun getPluginId(portIndex: Int, acceptAnyIndexForSinglePlugin: Boolean = true): String {
        val deviceName = deviceInfo.properties.get(MidiDeviceInfo.PROPERTY_NAME)?.toString() ?: ""
        val portName = deviceInfo.ports[portIndex].name
        val plugin = plugins.firstOrNull { plugin ->
            plugin.displayName.startsWith(deviceName) && plugin.displayName.endsWith(portName) }
        assert (plugin != null || acceptAnyIndexForSinglePlugin && plugins.size == 1)
        return (plugin ?: plugins.first()).pluginId!!
    }

    // It is designed to be open overridable.
    open val plugins: List<PluginInformation>
        get() = AudioPluginServiceHelper.getLocalAudioPluginService(applicationContext).plugins
            .filter { p -> isInstrument(p) }

    private fun isInstrument(info: PluginInformation) : Boolean {
        val c = info.category
        return c != null && (c.contains(PluginInformation.PRIMARY_CATEGORY_INSTRUMENT) || c.contains("Synth"))
    }

    override fun onCreate() {
        super.onCreate()

        portStatus = Array(deviceInfo.inputPortCount) { false }
    }

    private var receivers: Array<MidiReceiver>? = null

    override fun onGetInputPortReceivers(): Array<MidiReceiver> {
        receivers = receivers ?: (0 until deviceInfo.inputPortCount).map { AudioPluginMidiReceiver(this, it) }.toTypedArray()
        return receivers!!
    }

    private lateinit var portStatus: Array<Boolean>

    override fun onDeviceStatusChanged(status: MidiDeviceStatus) {
        super.onDeviceStatusChanged(status)

        for (i in 0 until deviceInfo.inputPortCount) {
            if (status.isInputPortOpen(i) == portStatus[i])
                continue
            portStatus[i] = !portStatus[i]
            if (portStatus[i])
                (receivers!![i] as AudioPluginMidiReceiver).onDeviceOpened()
            else
                (receivers!![i] as AudioPluginMidiReceiver).onDeviceClosed()
        }
    }
}
