package org.androidaudioplugin.ui.web

import android.webkit.JavascriptInterface
import org.androidaudioplugin.hosting.NativeRemotePluginInstance
import java.nio.ByteBuffer

/**
 * `AAPClientScriptInterface` is a reference `AAPScriptInterface` implementation which provides JavaScript API
 * to interoperate with the AudioPluginInstance. It is intended to be used for in-host-process UI.
 *
 * It should store the GUI events into the plugin's event input buffer, which should be -
 *
 * - "unified" with the audio plugin's process() event inputs, when the plugin is "activated"
 * - immediately processed when it is not at "activated" state.
 *
 * The AudioPluginInstance is responsible to store those event inputs and deal with above.
 */
@Suppress("unused")
class AAPClientScriptInterface(private val instance: NativeRemotePluginInstance) : AAPScriptInterface() {
    override fun addEventUmpInput(data: ByteBuffer, size: Int) {
        instance.addEventUmpInput(data, size)
    }

    override fun getPortBuffer(port: Int, data: ByteArray, length: Int) =
        instance.getPortBuffer(port, ByteBuffer.wrap(data, 0, length), length)

    // It seems that those JavaScriptInterface attributes are also needed for overriden members...

    @get:JavascriptInterface
    override val portCount = instance.getPortCount()

    @JavascriptInterface
    override fun getPort(index: Int) = JsPortInformation(instance.getPort(index))

    @get:JavascriptInterface
    override val parameterCount = instance.getParameterCount()

    @JavascriptInterface
    override fun getParameter(id: Int) = JsParameterInformation(instance.getParameter(id))
}