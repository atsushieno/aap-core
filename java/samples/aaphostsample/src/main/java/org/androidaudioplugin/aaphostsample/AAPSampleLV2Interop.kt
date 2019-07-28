package org.androidaudioplugin.aaphostsample

public class AAPSampleLV2Interop {
    companion object {
        init {
            System.loadLibrary("androidaudioplugin")
            System.loadLibrary("aaphostsample")
        }

        @JvmStatic
        external fun runHostLilv(plugins: Array<String>, sampleRate: Int, waveIn: ByteArray, waveOut: ByteArray)
        @JvmStatic
        external fun runHostAAP(plugins: Array<String>, sampleRate: Int, waveIn: ByteArray, waveOut: ByteArray)
    }
}