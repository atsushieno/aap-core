package org.androidaudioplugin.hosting

import android.content.Context
import androidx.startup.Initializer
import org.androidaudioplugin.AudioPluginNatives

/**
 * Implements an Initializer in Jetpack App Startup manner. Every host should set it up.
 *
 * libandroidaudioplugin.so may be dynamically loaded when any library that depends on it is loaded,
 * but in that case the initialization function set up here is NOT called and thus JNI calls
 * (including the  basic AAP Binder calls) will fail.
 * To avoid that, set up this initializer to ensure the JNI setup.
 */
class AudioPluginClientInitializer : Initializer<Unit> {
    override fun create(context: Context) : Unit {
        System.loadLibrary("androidaudioplugin")
        AudioPluginNatives.initializeAAPJni(context)
    }

    override fun dependencies(): MutableList<Class<out Initializer<*>>> {
        return mutableListOf<Class<out Initializer<*>>>()
    }
}

