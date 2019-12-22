package org.androidaudioplugin

import android.content.Context
import android.util.Log

class AudioPluginLocalHost : AudioPluginService() {
    companion object {

        init {
            System.loadLibrary("androidaudioplugin")
        }

        var initialized: Boolean = false

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
                var method = c.getMethod("initialize", Context::class.java)
                method.invoke(null, context)
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
            cleanupNatives()
            initialized = false
        }

        @JvmStatic
        external fun cleanupNatives()

        @JvmStatic
        fun getLocalAudioPluginService(context: Context) = AudioPluginHost.queryAudioPluginServices(
            context
        ).first { svc -> svc.packageName == context.packageName }
    }
}