package org.androidaudiopluginframework.hosting

import android.content.res.AssetManager

class AAPLV2LocalHost
{
    companion object {
        init {
            System.loadLibrary("androidaudioplugin")
        }

        @JvmStatic
        external fun initialize(pluginPaths: Array<String>, assets: AssetManager)

        @JvmStatic
        external fun cleanup()

        @JvmStatic
        external fun runHost(pluginUris: Array<String>, wav: ByteArray, outWav: ByteArray) : Int
    }
}