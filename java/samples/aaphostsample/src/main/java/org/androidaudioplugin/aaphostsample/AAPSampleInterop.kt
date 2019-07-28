package org.androidaudioplugin.aaphostsample

import android.os.IBinder

public class AAPSampleInterop {
    companion object {
        @JvmStatic
        external fun runClientAAP(binder: IBinder, sampleRate: Int, pluginId: String, waveIn: ByteArray, waveOut: ByteArray)
    }
}