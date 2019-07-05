package org.androidaudiopluginframework.hosting

import android.content.res.AssetManager
import org.androidaudiopluginframework.PluginInformation

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
        external fun runHostLilv(pluginUris: Array<String>, wav: ByteArray, outWav: ByteArray) : Int

        @JvmStatic
        external fun runHostAAP(pluginInfos: Array<PluginInformation>, pluginUris: Array<String>, sampleRate: Int, wav: ByteArray, outWav: ByteArray) : Int
    }
}