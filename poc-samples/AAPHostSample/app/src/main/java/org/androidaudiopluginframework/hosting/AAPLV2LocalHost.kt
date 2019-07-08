package org.androidaudiopluginframework.hosting

import android.content.Context
import android.content.res.AssetManager
import org.androidaudiopluginframework.AudioPluginHost
import org.androidaudiopluginframework.PluginInformation

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

        fun runHostAAP(context: Context, pluginUris: Array<String>, sampleRate: Int, wav: ByteArray, outWav: ByteArray) : Int
        {
            var pluginInfos = AudioPluginHost.queryAudioPluginServices(context).flatMap { i -> i.plugins }.toTypedArray()
            return runHostAAP(pluginInfos, pluginUris, sampleRate, wav, outWav)
        }

        @JvmStatic
        external fun runHostAAP(pluginInfos: Array<PluginInformation>, pluginUris: Array<String>, sampleRate: Int, wav: ByteArray, outWav: ByteArray) : Int
    }
}