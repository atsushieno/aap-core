package org.androidaudioplugin

import android.content.Context
import org.androidaudioplugin.hosting.AudioPluginHostHelper

// It is used only by AudioPluginService and some plugin extensions (such as AudioPluginLv2ServiceExtension) to process client (plugin host) requests.
internal object AudioPluginServiceHelper {
    fun getLocalAudioPluginService(context: Context) =
        AudioPluginHostHelper.queryAudioPluginServices(context)
            .first { svc -> svc.packageName == context.packageName }
}
