package org.androidaudioplugin.androidaudioplugin.testing

import android.content.Context
import android.media.midi.MidiDeviceInfo
import android.media.midi.MidiManager
import kotlinx.coroutines.delay
import kotlinx.coroutines.runBlocking

class AudioPluginMidiDeviceServiceTesting(private val applicationContext: Context) {

    val simpleNoteOn = arrayOf(0x90, 0x40, 10).map { it.toByte() }.toByteArray()
    val simpleNoteOff = arrayOf(0x80, 0x40, 0).map { it.toByte() }.toByteArray()

    fun basicServiceOperations(deviceName: String) {
        val midiManager = applicationContext.getSystemService(Context.MIDI_SERVICE) as MidiManager
        val deviceInfo = midiManager.devices.first { d -> d.properties.getString(MidiDeviceInfo.PROPERTY_NAME) == deviceName }

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
