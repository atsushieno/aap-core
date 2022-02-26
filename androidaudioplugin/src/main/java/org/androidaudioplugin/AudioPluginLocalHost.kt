package org.androidaudioplugin

import android.content.Context
import org.androidaudioplugin.hosting.AudioPluginHostHelper

// It is used only by AudioPluginService and some plugin extensions (such as AudioPluginLv2ServiceExtension) to process client (plugin host) requests.
internal class AudioPluginLocalHost {
    companion object {
        var initialized: Boolean = false
        var extensions = mutableListOf<AudioPluginService.Extension>()

        @JvmStatic
        fun initialize(context: Context)
        {
            val si = getLocalAudioPluginService(context)
            si.extensions.forEach { e ->
                if(e == "")
                    return
                val c = Class.forName(e)
                val ext = c.newInstance() as AudioPluginService.Extension
                ext.initialize(context)
                extensions.add(ext)
            }
            AudioPluginNatives.initializeAAPJni(context.applicationContext)
            AudioPluginNatives.initializeLocalHostForPluginService()
            initialized = true
        }

        @JvmStatic
        fun cleanup()
        {
            for(ext in extensions)
                ext.cleanup()
            AudioPluginNatives.cleanupLocalHostNatives()
            initialized = false
        }

        @JvmStatic
        fun getLocalAudioPluginService(context: Context) = AudioPluginHostHelper.queryAudioPluginServices(
            context
        ).first { svc -> svc.packageName == context.packageName }
    }
}
