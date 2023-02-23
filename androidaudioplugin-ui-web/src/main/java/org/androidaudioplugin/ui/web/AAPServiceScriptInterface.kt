package org.androidaudioplugin.ui.web

import android.webkit.JavascriptInterface
import org.androidaudioplugin.NativeLocalPluginInstance
import java.nio.ByteBuffer

/**
 * `AAPServiceScriptInterface` is an `AAPScriptInterface` which provides JavaScript API to interoperate
 * with the AudioPluginService via AudioPluginWevViewFactory.
 *
 * It should store the GUI events into the plugin's event input buffer, which should be -
 *
 * - "unified" with the audio plugin's process() event inputs, when the plugin is "activated"
 * - immediately processed when it is not at "activated" state.
 *
 * The NativeLocalPluginInstance is responsible to store those event inputs and deal with above.
 */
class AAPServiceScriptInterface(private val instance: NativeLocalPluginInstance) : AAPScriptInterface() {
    private val DEFAULT_BUFFER_SIZE = 8192

    override fun sendMidi1(data: ByteArray, offset: Int, length: Int) {
        TODO("Not yet implemented")
    }

    override fun setParameter(parameterId: Int, value: Double) {
        TODO("Not yet implemented")
    }

    override fun getPortBuffer(port: Int, data: ByteArray, length: Int) {
        TODO("Not yet implemented")
    }

    @get:JavascriptInterface
    override val portCount = instance.getPortCount()

    @JavascriptInterface
    override fun getPort(index: Int) = JsPortInformation(instance.getPort(index))

    @get:JavascriptInterface
    override val parameterCount = instance.getParameterCount()

    @JavascriptInterface
    override fun getParameter(index: Int) = JsParameterInformation(instance.getParameter(index))
}