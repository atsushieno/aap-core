package org.androidaudioplugin.hosting

import android.content.Context
import androidx.startup.Initializer
import org.androidaudioplugin.AudioPluginNatives

class AudioPluginClientInitializer : Initializer<Unit> {
    override fun create(context: Context) : Unit {
        System.loadLibrary("androidaudioplugin")
        AudioPluginNatives.initializeAAPJni(context)
    }

    override fun dependencies(): MutableList<Class<out Initializer<*>>> {
        return mutableListOf<Class<out Initializer<*>>>()
    }
}

