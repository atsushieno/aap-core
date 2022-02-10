package org.androidaudioplugin.midideviceservice

import android.media.midi.MidiReceiver

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

    private var instance: AudioPluginMidiDeviceInstance? = null

    fun onDeviceOpened(pluginId: String) {
        assert(instance == null)
        instance = AudioPluginMidiDeviceInstance(pluginId, ownerService)
    }

    fun onDeviceClosed() {
        assert(instance != null)
        instance?.onDeviceClosed()
        instance = null
    }

    override fun onSend(msg: ByteArray?, offset: Int, count: Int, timestamp: Long) {
        instance?.onSend(msg, offset, count, timestamp)
    }
}

