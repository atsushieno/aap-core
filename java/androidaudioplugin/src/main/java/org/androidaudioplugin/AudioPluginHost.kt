package org.androidaudioplugin

import android.content.Context
import android.os.IBinder

class AudioPluginHost(context: Context) {
    companion object {
        init {
            System.loadLibrary("androidaudioplugin")
        }

        @JvmStatic
        external fun setApplicationContext(applicationContext: Context)

        @JvmStatic
        external fun initialize(pluginInfos: Array<PluginInformation>)
    }

    var connectedBinders = mutableListOf<IBinder>()

    init {
        setApplicationContext(context)
        initialize(AudioPluginHostHelper.queryAudioPluginServices(context).flatMap { s -> s.plugins }.toTypedArray())
    }

    fun connect(binder: IBinder) {
        if (connectedBinders.any { c -> c.interfaceDescriptor == binder.interfaceDescriptor })
            return
        connectedBinders.add(binder)
    }

    fun disconnect(binder: IBinder) {
        var existing = connectedBinders.firstOrNull { c -> c.interfaceDescriptor == binder.interfaceDescriptor }
        if (existing == null)
            return
        // Do we need this?
        existing.unlinkToDeath({}, 0)
    }
}
