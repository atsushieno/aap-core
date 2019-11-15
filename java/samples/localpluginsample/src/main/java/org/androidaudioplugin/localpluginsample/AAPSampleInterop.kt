package org.androidaudioplugin.localpluginsample

import android.os.IBinder

public class AAPSampleInterop {
    companion object {
        init {
            System.loadLibrary("androidaudioplugin")
            System.loadLibrary("localpluginsample")
        }

        @JvmStatic
        external fun runClientAAP(binder: IBinder, sampleRate: Int, pluginId: String, audioInL: ByteArray, audioInR: ByteArray, audioOutL: ByteArray, audioOutR: ByteArray)
    }
}
