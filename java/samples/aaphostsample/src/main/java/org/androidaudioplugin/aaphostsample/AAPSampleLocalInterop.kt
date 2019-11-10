package org.androidaudioplugin.aaphostsample

public class AAPSampleLocalInterop {
    companion object {
        init {
            System.loadLibrary("androidaudioplugin")
            System.loadLibrary("aaphostsample")
        }
        @JvmStatic
        external fun runHostAAP(plugins: Array<String>, sampleRate: Int, waveIn: ByteArray, waveOut: ByteArray)
    }
}