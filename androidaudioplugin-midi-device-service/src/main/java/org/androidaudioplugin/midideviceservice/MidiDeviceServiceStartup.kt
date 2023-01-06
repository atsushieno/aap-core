package org.androidaudioplugin.midideviceservice

import android.content.Context
import androidx.startup.Initializer
import org.androidaudioplugin.hosting.AudioPluginHostHelper
import org.androidaudioplugin.hosting.AudioPluginMidiSettings

class MidiDeviceServiceStartup : Initializer<Unit> {
    override fun create(context: Context) {
        System.loadLibrary("aapmidideviceservice")
        initializeApplicationContext(context.applicationContext)
    }

    private external fun initializeApplicationContext(context: Context)

    override fun dependencies(): MutableList<Class<out Initializer<*>>> = mutableListOf()
}
