package org.androidaudioplugin.ui.web

import android.webkit.JavascriptInterface
import org.androidaudioplugin.NativeLocalPluginInstance

class AAPServiceScriptInterface(private val instance: NativeLocalPluginInstance) : AAPScriptInterface() {

    @get:JavascriptInterface
    override val portCount = instance.getPortCount()

    @JavascriptInterface
    override fun getPort(index: Int) = JsPortInformation(instance.getPort(index))

    @get:JavascriptInterface
    override val parameterCount = instance.getParameterCount()

    @JavascriptInterface
    override fun getParameter(index: Int) = JsParameterInformation(instance.getParameter(index))
}