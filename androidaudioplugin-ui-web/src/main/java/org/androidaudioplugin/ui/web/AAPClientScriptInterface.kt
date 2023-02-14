package org.androidaudioplugin.ui.web

import android.webkit.JavascriptInterface
import org.androidaudioplugin.hosting.AudioPluginInstance

@Suppress("unused")
class AAPClientScriptInterface(private val instance: AudioPluginInstance) : AAPScriptInterface() {

    // It seems that those JavaScriptInterface attributes are also needed for overriden members...

    @get:JavascriptInterface
    override val portCount: Int
        get() = instance.getPortCount()

    @JavascriptInterface
    override fun getPort(index: Int) = JsPortInformation(instance.getPort(index))

    @get:JavascriptInterface
    override val parameterCount: Int
        get() = instance.getParameterCount()

    @JavascriptInterface
    override fun getParameter(id: Int) = JsParameterInformation(instance.getParameter(id))
}