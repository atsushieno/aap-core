package org.androidaudioplugin.androidaudioplugin.testing

import android.content.Context
import android.media.midi.MidiDeviceInfo
import android.media.midi.MidiManager
import kotlinx.coroutines.delay
import kotlinx.coroutines.runBlocking
import org.androidaudioplugin.AudioPluginHostHelper
import org.androidaudioplugin.PluginInformation

class AudioPluginMidiDeviceServiceTesting(private val applicationContext: Context) {
    fun basicServiceOperationsForAllPlugins() {
        for (pluginInfo in AudioPluginHostHelper.getLocalAudioPluginService(applicationContext).plugins)
            testInstancingAndProcessing(pluginInfo)
    }

    val simpleNoteOn = arrayOf(0x90, 0x40, 10).map { it.toByte() }.toByteArray()
    val simpleNoteOff = arrayOf(0x80, 0x40, 0).map { it.toByte() }.toByteArray()

    fun testInstancingAndProcessing(pluginInfo: PluginInformation) {
        val midiManager = applicationContext.getSystemService(Context.MIDI_SERVICE) as MidiManager
        val deviceInfo = midiManager.devices.first { d -> d.properties.getString(MidiDeviceInfo.PROPERTY_NAME) == pluginInfo.displayName }

        midiManager.openDevice(deviceInfo, { device ->
            runBlocking {
                val input = device.openInputPort(deviceInfo.ports.first { p ->
                    p.type == MidiDeviceInfo.PortInfo.TYPE_INPUT
                }.portNumber)
                input.send(simpleNoteOn, 0, simpleNoteOn.size)
                delay(50)
                input.send(simpleNoteOff, 0, simpleNoteOn.size)
            }
        }, null)
    }
}
