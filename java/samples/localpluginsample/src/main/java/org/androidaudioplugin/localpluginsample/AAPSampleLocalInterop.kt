package org.androidaudioplugin.localpluginsample

public class AAPSampleLocalInterop {
    companion object {
        init {
            System.loadLibrary("androidaudioplugin")
            System.loadLibrary("localpluginsample")
        }
        @JvmStatic
        external fun runHostAAP(plugins: Array<String>, sampleRate: Int, waveIn: ByteArray, waveOut: ByteArray)
    }
}
