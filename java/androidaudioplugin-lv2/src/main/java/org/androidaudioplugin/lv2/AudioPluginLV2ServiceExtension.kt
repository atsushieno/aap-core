package org.androidaudioplugin.lv2

import android.content.Context
import android.content.res.AssetManager
import org.androidaudioplugin.AudioPluginLocalHost
import org.androidaudioplugin.AudioPluginService

// This class is (can be) used as an AudioPluginHost extension.
class AudioPluginLV2ServiceExtension : AudioPluginService.Extension
{
    companion object {
        init {
            System.loadLibrary("androidaudioplugin")
            System.loadLibrary("androidaudioplugin-lv2")
        }
    }

    override fun initialize(context: Context)
    {
        var lv2Paths = AudioPluginLocalHost.getLocalAudioPluginService(context).plugins
            .filter { p -> p.backend == "LV2" }.map { p -> if(p.assets != null) p.assets!! else "" }
            .distinct().toTypedArray()
        initialize(lv2Paths.joinToString(":"), context.assets)
    }

    private external fun initialize(lv2Path: String, assets: AssetManager)

    external override fun cleanup()
}