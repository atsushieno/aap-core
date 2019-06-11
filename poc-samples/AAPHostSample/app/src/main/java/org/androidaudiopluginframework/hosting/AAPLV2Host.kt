package org.androidaudiopluginframework.hosting

import android.content.res.AssetManager

class AAPLV2Host
{
    companion object {
        init {
            System.loadLibrary("aaphost")
        }

        @JvmStatic
        external fun runHost(assets: AssetManager, pluginUris: Array<String>) : Int
        @JvmStatic
        external fun runHostOne(assets: AssetManager, pluginUri: String) : Int
    }
}