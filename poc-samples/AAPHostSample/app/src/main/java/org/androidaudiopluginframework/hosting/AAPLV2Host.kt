package org.androidaudiopluginframework.hosting

import android.content.res.AssetManager

class AAPLV2Host
{
    companion object {
        init {
            System.loadLibrary("androidaudioplugin")
        }

        @JvmStatic
        external fun runHost(pluginPaths: Array<String>, assets: AssetManager, pluginUris: Array<String>) : Int
    }
}