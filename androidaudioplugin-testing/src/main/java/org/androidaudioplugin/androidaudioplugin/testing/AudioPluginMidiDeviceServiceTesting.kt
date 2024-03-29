package org.androidaudioplugin.androidaudioplugin.testing

import android.content.Context
import android.media.midi.MidiDeviceInfo
import android.media.midi.MidiManager
import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.delay
import kotlinx.coroutines.runBlocking
import kotlinx.coroutines.sync.Mutex

class AudioPluginMidiDeviceServiceTesting(private val applicationContext: Context) {

    val simpleNoteOn = arrayOf(0x90, 0x40, 10).map { it.toByte() }.toByteArray()
    val simpleNoteOff = arrayOf(0x80, 0x40, 0).map { it.toByte() }.toByteArray()

    fun basicServiceOperations(deviceName: String) {
        val midiManager = applicationContext.getSystemService(Context.MIDI_SERVICE) as MidiManager
        val deviceInfo = midiManager.devices.first { d -> d.properties.getString(MidiDeviceInfo.PROPERTY_NAME) == deviceName }

        runBlocking {
            val deferred = CompletableDeferred<Unit>()

            midiManager.openDevice(deviceInfo, { device ->
                var error: Exception? = null
                runBlocking {
                    try {
                        val input = device.openInputPort(deviceInfo.ports.first { p ->
                            p.type == MidiDeviceInfo.PortInfo.TYPE_INPUT
                        }.portNumber)
                        assert(input != null)
                        input.send(simpleNoteOn, 0, simpleNoteOn.size)
                        delay(50)
                        input.send(simpleNoteOff, 0, simpleNoteOn.size)

                        input.close()
                    } catch (ex: Exception) {
                        error = ex
                    } finally {
                        deferred.complete(Unit)
                    }
                    if (error != null)
                        throw AssertionError("MIDI output test failure", error)

                    device.close()
                }
            }, null)

            deferred.await()
        }
    }
}
