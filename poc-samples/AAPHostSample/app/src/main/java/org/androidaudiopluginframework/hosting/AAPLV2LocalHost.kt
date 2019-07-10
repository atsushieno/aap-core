package org.androidaudiopluginframework.hosting

import android.content.res.AssetManager

class AAPLV2LocalHost
{
    companion object {
        init {
            System.loadLibrary("androidaudioplugin")
        }

        var lv2Paths : Array<String> = arrayOf()

        fun initialize(assets: AssetManager)
        {
            initialize(lv2Paths.joinToString(":"), assets)
        }

        @JvmStatic
        external fun initialize(lv2Path: String, assets: AssetManager)

        @JvmStatic
        external fun cleanup()

        // It is not really for public use.
        @JvmStatic
        external fun runHostLilv(pluginUris: Array<String>, sampleRate: Int, wav: ByteArray, outWav: ByteArray) : Int

        @JvmStatic
        external fun runHostAAP(pluginUris: Array<String>, sampleRate: Int, wav: ByteArray, outWav: ByteArray) : Int
    }
}