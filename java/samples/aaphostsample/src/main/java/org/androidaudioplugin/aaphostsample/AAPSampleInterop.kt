package org.androidaudioplugin.aaphostsample

import android.os.IBinder

public class AAPSampleInterop {
    companion object {
        init {
            System.loadLibrary("androidaudioplugin")
            System.loadLibrary("aaphostsample")
        }

        @JvmStatic
        external fun runClientAAP(binder: IBinder, sampleRate: Int, pluginId: String, waveIn: ByteArray, waveOut: ByteArray)
    }
}