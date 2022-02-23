package org.androidaudioplugin.midideviceservice

import android.content.Context
import androidx.startup.Initializer

class MidiDeviceServiceStartup : Initializer<Unit> {
    override fun create(context: Context) {
        System.loadLibrary("aapmidideviceservice")
        initializeApplicationContext(context)
    }

    private external fun initializeApplicationContext(context: Context)

    override fun dependencies(): MutableList<Class<out Initializer<*>>> = mutableListOf()
}
