package org.androidaudioplugin.lv2

import android.content.Context
import android.content.res.AssetManager
import org.androidaudioplugin.AudioPluginHost

class AudioPluginLV2LocalHost
{
    companion object {
        init {
            System.loadLibrary("androidaudioplugin")
            // FIXME: bring it back once we managed to split those native lib builds
            //System.loadLibrary("androidaudioplugin-lv2")
        }

        fun initialize(context: Context)
        {
            var lv2Paths = AudioPluginHost.queryLocalAudioPlugins(context)
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