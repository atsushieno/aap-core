package org.androidaudioplugin

import android.content.Context
import android.os.IBinder
import java.io.Closeable

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

    var connectedServices = mutableListOf<AudioPluginHostConnection>()

    init {
        setApplicationContext(context)
        initialize(AudioPluginHostHelper.queryAudioPluginServices(context).flatMap { s -> s.plugins }.toTypedArray())
    }

    fun connect(binder: IBinder) {
        if (connectedServices.any { c -> c.binder.interfaceDescriptor == binder.interfaceDescriptor })
            return
        connectedServices.add(AudioPluginHostConnection(binder))
    }

    fun disconnect(binder: IBinder) {
        var existing = connectedServices.firstOrNull { c -> c.binder.interfaceDescriptor == binder.interfaceDescriptor }
        if (existing == null)
            return
        existing.close()
        connectedServices.remove(existing)
    }
}

class AudioPluginHostConnection(binder: IBinder) : Closeable
{
    companion object {
        @JvmStatic
        external fun connect(binder : IBinder) : Long
        @JvmStatic
        external fun disconnect(handle: Long)
    }

    var binder : IBinder = binder
    private var handle : Long;

    init {
        handle = connect(binder)
    }

    override fun close() {
        disconnect(handle)
    }
}
