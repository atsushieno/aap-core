package org.androidaudioplugin.ui.web

import android.content.Context
import android.view.View
import org.androidaudioplugin.AudioPluginViewFactory

class AudioPluginWebViewFactory : AudioPluginViewFactory() {
    override fun createView(context: Context, pluginId: String, instanceId: Int): View {
        TODO("implement alternative to WebUI*Host*Helper (we'd need something comparable to AudioPluginInstance but for local service instance)")
        //return WebUIHostHelper.getWebView(context)
    }
}