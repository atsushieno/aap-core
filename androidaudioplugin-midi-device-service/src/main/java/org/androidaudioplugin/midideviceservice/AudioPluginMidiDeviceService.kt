package org.androidaudioplugin.midideviceservice

import android.content.Context
import android.media.midi.MidiDeviceInfo
import android.media.midi.MidiDeviceService
import android.media.midi.MidiDeviceStatus
import android.media.midi.MidiReceiver
import android.media.midi.MidiUmpDeviceService
import androidx.annotation.RequiresApi
import org.androidaudioplugin.*

@RequiresApi(35)
abstract class AudioPluginMidiUmpDeviceService : MidiUmpDeviceService() {
    private val impl by lazy { AudioPluginMidi2Device(this) }

    // It is designed to be open overridable.
    abstract val plugins: List<PluginInformation>

    override fun onGetInputPortReceivers() = impl.onGetInputPortReceivers().toMutableList()
    override fun onDeviceStatusChanged(status: MidiDeviceStatus) {
        super.onDeviceStatusChanged(status)
        impl.onDeviceStatusChanged(status)
    }
}

abstract class AudioPluginMidiDeviceService : MidiDeviceService() {
    private val impl by lazy { AudioPluginMidi1Device(this) }

    // It is designed to be open overridable.
    abstract val plugins: List<PluginInformation>

    override fun onGetInputPortReceivers(): Array<MidiReceiver> = impl.onGetInputPortReceivers()
    override fun onDeviceStatusChanged(status: MidiDeviceStatus) {
        super.onDeviceStatusChanged(status)
        impl.onDeviceStatusChanged(status)
    }
}

internal class AudioPluginMidi1Device(private val owner: AudioPluginMidiDeviceService)
    : AudioPluginMidiDevice({ owner.applicationContext }, { owner.deviceInfo }, owner.plugins) {

    override val midiProtocol = 1

    override fun getOutputPortReceiver(portIndex: Int): MidiReceiver? {
        val receivers = owner.getOutputPortReceivers()
        return if (portIndex < receivers.size) receivers[portIndex] else null
    }
}

@RequiresApi(35)
internal class AudioPluginMidi2Device(private val owner: AudioPluginMidiUmpDeviceService)
    : AudioPluginMidiDevice({ owner.applicationContext }, { owner.deviceInfo!! }, owner.plugins) {

    override val midiProtocol = 2

    override fun getOutputPortReceiver(portIndex: Int): MidiReceiver? {
        val receivers = owner.getOutputPortReceivers()
        return if (portIndex < receivers.size) receivers[portIndex] else null
    }
}

abstract class AudioPluginMidiDevice(
    lazyGetApplicationContext: ()->Context,
    lazyGetDeviceInfo: ()->MidiDeviceInfo,
    candidatePlugins: List<PluginInformation>
) {
    val applicationContext by lazy { lazyGetApplicationContext() }
    val deviceInfo by lazy { lazyGetDeviceInfo() }

    val plugins: List<PluginInformation> = candidatePlugins.filter { p -> isInstrument(p) }
    private fun isInstrument(info: PluginInformation) : Boolean {
        val c = info.category
        return c != null && (c.contains(PluginInformation.PRIMARY_CATEGORY_INSTRUMENT) || c.contains("Synth"))
    }

    abstract val midiProtocol: Int

    private val receivers: Array<MidiReceiver> by lazy {
        (0 until deviceInfo.inputPortCount).map {
            AudioPluginMidiReceiver(this, it, midiProtocol) }.toTypedArray()
    }

    fun onGetInputPortReceivers(): Array<MidiReceiver> = receivers

    private val portStatus by lazy { Array(deviceInfo.inputPortCount) { false } }

    fun onDeviceStatusChanged(status: MidiDeviceStatus) {
        for (i in 0 until deviceInfo.inputPortCount) {
            if (status.isInputPortOpen(i) == portStatus[i])
                continue
            portStatus[i] = !portStatus[i]
            if (portStatus[i])
                (receivers[i] as AudioPluginMidiReceiver).onDeviceOpened()
            else
                (receivers[i] as AudioPluginMidiReceiver).onDeviceClosed()
        }
    }

    /**
     * Returns the [MidiReceiver] for the device's MIDI output port at [portIndex], or null if
     * the device has no output port at that index.
     *
     * Concrete subclasses obtain this from their owning service's [getOutputPortReceivers] array
     * (inherited from [MidiDeviceService] / [MidiUmpDeviceService]).
     */
    abstract fun getOutputPortReceiver(portIndex: Int): MidiReceiver?

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
}
