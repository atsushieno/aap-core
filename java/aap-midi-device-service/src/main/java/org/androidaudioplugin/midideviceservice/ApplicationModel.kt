package org.androidaudioplugin.midideviceservice

import android.app.Service
import android.content.Context
import android.media.midi.MidiDevice
import android.media.midi.MidiDeviceInfo
import android.media.midi.MidiInputPort
import android.media.midi.MidiManager
import android.media.midi.MidiManager.OnDeviceOpenedListener
import kotlinx.coroutines.GlobalScope
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import org.androidaudioplugin.AudioPluginHostHelper
import org.androidaudioplugin.PluginInformation


internal lateinit var applicationContextForModel: Context

val model: ApplicationModel
    get() = ApplicationModel.instance

internal const val SHARED_PREFERENCE_KEY = "aap-midi-device-service-preferences"
internal const val PREFERENCE_KRY_PLUGIN_ID = "last_used_plugin_id"
internal const val PREFERENCE_KRY_USED_MIDI2_PROTOCOL = "last_used_midi2_protocol"

class ApplicationModel(private val packageName: String, context: Context) {
    companion object {
        val instance = ApplicationModel(applicationContextForModel.packageName, applicationContextForModel)
    }

    val midiManager = context.getSystemService(Service.MIDI_SERVICE) as MidiManager
    lateinit var midiInput: MidiInputPort
    val pluginServices = AudioPluginHostHelper.queryAudioPluginServices(context.applicationContext)

    var midiManagerInitialized = false

    private var _useMidi2Protocol = false
    var useMidi2Protocol : Boolean
        get() = _useMidi2Protocol
        set(value) {
            _useMidi2Protocol = value
            val sp = applicationContextForModel.getSharedPreferences(SHARED_PREFERENCE_KEY, Context.MODE_PRIVATE)
            sp.edit().putBoolean(PREFERENCE_KRY_USED_MIDI2_PROTOCOL, value).apply()
        }

    private var _specifiedInstrument: PluginInformation? = null
    var specifiedInstrument: PluginInformation?
        get() = _specifiedInstrument
        set(value) {
            _specifiedInstrument = value
            // save instrument as the last used one, so that it can be the default.
            val sp = applicationContextForModel.getSharedPreferences(SHARED_PREFERENCE_KEY, Context.MODE_PRIVATE)
            sp.edit().putString(PREFERENCE_KRY_PLUGIN_ID, value?.pluginId).apply()
        }

    private fun getLastUsedMidi2(ctx: Context) : Boolean {
        val sp = ctx.getSharedPreferences(SHARED_PREFERENCE_KEY, Context.MODE_PRIVATE)
        return if (sp.contains(PREFERENCE_KRY_USED_MIDI2_PROTOCOL))
            sp.getBoolean(PREFERENCE_KRY_USED_MIDI2_PROTOCOL, false)
        else false
    }

    private fun getLastUsedInstrument(ctx: Context): PluginInformation? {
        val sp = ctx.getSharedPreferences(SHARED_PREFERENCE_KEY, Context.MODE_PRIVATE)
        val pluginId = sp.getString(PREFERENCE_KRY_PLUGIN_ID, null)
        if (pluginId != null)
            return pluginServices.flatMap { s -> s.plugins }
                .firstOrNull { p -> p.pluginId == pluginId }
        return null
    }

    private val allPlugins: Array<PluginInformation> = AudioPluginHostHelper.queryAudioPlugins(context)

    val instrument: PluginInformation
        get() = specifiedInstrument ?:
            allPlugins.firstOrNull { p -> p.packageName == packageName && isInstrument(p) } ?:
            allPlugins.first { p -> isInstrument(p) }

    private fun isInstrument(info: PluginInformation) =
        info.category?.contains(PluginInformation.PRIMARY_CATEGORY_INSTRUMENT) ?: info.category?.contains("Synth") ?: false

    // it is not implemented to be restartable yet
    fun terminateMidi() {
        if (!midiManagerInitialized)
            return
        midiInput.close()
        midiManagerInitialized = false
    }

    fun initializeMidi() {
        if (midiManagerInitialized)
            return

        val deviceInfo: MidiDeviceInfo = midiManager.devices.first { d ->
            d.properties.getString(MidiDeviceInfo.PROPERTY_MANUFACTURER) == "androidaudioplugin.org" &&
                    d.properties.getString(MidiDeviceInfo.PROPERTY_NAME) == "AAPMidi"
        }
        midiManager.openDevice(deviceInfo, object: OnDeviceOpenedListener {
            override fun onDeviceOpened(device: MidiDevice?) {
                if (device == null)
                    return
                midiInput = device.openInputPort(0) ?: throw Exception("failed to open input port")
                midiManagerInitialized = true
            }
        }, null)
    }

    fun playNote() {
        if (!midiManagerInitialized)
            return

        GlobalScope.launch {
            if (useMidi2Protocol) {
                midiInput.send(byteArrayOf(0x40, 0x90.toByte(), 60, 0, 100, 0, 0, 0), 0, 8)
                delay(1000)
                midiInput.send(byteArrayOf(0x40, 0x80.toByte(), 60, 0, 100, 0, 0, 0), 0, 8)
            } else {
                midiInput.send(byteArrayOf(0x90.toByte(), 60, 100), 0, 3)
                delay(1000)
                midiInput.send(byteArrayOf(0x80.toByte(), 60, 0), 0, 3)
            }
        }
    }

    init {
        specifiedInstrument = getLastUsedInstrument(context)
        useMidi2Protocol = getLastUsedMidi2(context)
    }
}
