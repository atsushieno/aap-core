package org.androidaudioplugin

import android.content.Context
import android.util.Log

class AudioPluginLocalHost {
    companion object {

        init {
            System.loadLibrary("androidaudioplugin")
        }

        var initialized: Boolean = false
        var extensions = mutableListOf<AudioPluginService.Extension>()

        @JvmStatic
        fun initialize(context: Context)
        {
            var si = getLocalAudioPluginService(context)
            si.extensions.forEach { e ->
                if(e == null || e == "")
                    return
                var c = Class.forName(e)
                if (c == null) {
                    Log.d("AAP", "Extension class not found: " + e)
                    return@forEach
                }
                var ext = c.newInstance() as AudioPluginService.Extension
                ext.initialize(context)
                extensions.add(ext)
            }
            var pluginInfos = getLocalAudioPluginService(context).plugins.toTypedArray()
            initialize(pluginInfos)
            initialized = true
        }

        @JvmStatic
        external fun initialize(pluginInfos: Array<PluginInformation>)

        @JvmStatic
        fun cleanup()
        {
            for(ext in extensions)
                ext.cleanup()
            cleanupNatives()
            initialized = false
        }

        @JvmStatic
        external fun cleanupNatives()

        @JvmStatic
        fun getLocalAudioPluginService(context: Context) = AudioPluginHostHelper.queryAudioPluginServices(
            context
        ).first { svc -> svc.packageName == context.packageName }
    }
}