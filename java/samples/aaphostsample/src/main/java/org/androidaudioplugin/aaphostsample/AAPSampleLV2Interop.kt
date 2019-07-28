package org.androidaudioplugin.aaphostsample

public class AAPSampleLV2Interop {
    companion object {
        @JvmStatic
        external fun runHostLilv(plugins: Array<String>, sampleRate: Int, waveIn: ByteArray, waveOut: ByteArray)
        @JvmStatic
        external fun runHostAAP(plugins: Array<String>, sampleRate: Int, waveIn: ByteArray, waveOut: ByteArray)
    }
}