package org.androidaudioplugin.lv2

import android.content.Context
import android.content.res.AssetManager
import org.androidaudioplugin.AudioPluginHost

class AudioPluginLV2LocalHost
{
    companion object {
        init {
            System.loadLibrary("androidaudioplugin")
            System.loadLibrary("androidaudioplugin-lv2")
        }

        @JvmStatic
        fun initialize(context: Context)
        {
            var lv2Paths = AudioPluginHost.getLocalAudioPluginService(context).plugins
                .filter { p -> p.backend == "LV2" }.map { p -> if(p.assets != null) p.assets!! else "" }
                .distinct().toTypedArray()
            initialize(lv2Paths.joinToString(":"), context.assets)
        }

        @JvmStatic
        external fun initialize(lv2Path: String, assets: AssetManager)

        @JvmStatic
        external fun cleanup()
    }
}