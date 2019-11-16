package org.androidaudioplugin.aaphostsample

import android.os.IBinder
import org.androidaudioplugin.PluginInformation

public class AAPSampleInterop {
    companion object {
        init {
            System.loadLibrary("androidaudioplugin")
            System.loadLibrary("aaphostsample")
        }

        @JvmStatic
        external fun initialize(pluginInfos: Array<PluginInformation>)

        @JvmStatic
        external fun runClientAAP(binder: IBinder, sampleRate: Int, pluginId: String, audioInL: ByteArray, audioInR: ByteArray, audioOutL: ByteArray, audioOutR: ByteArray)
    }
}