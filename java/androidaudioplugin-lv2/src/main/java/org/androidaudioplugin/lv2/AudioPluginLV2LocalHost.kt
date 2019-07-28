package org.androidaudioplugin.lv2

import android.content.res.AssetManager

class AudioPluginLV2LocalHost
{
    companion object {
        init {
            System.loadLibrary("androidaudioplugin")
            // FIXME: bring it back once we managed to split those native lib builds
            //System.loadLibrary("androidaudioplugin-lv2")
            // FIXME: temporarily load from aaphostsample
            System.loadLibrary("aaphostsample")
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
    }
}