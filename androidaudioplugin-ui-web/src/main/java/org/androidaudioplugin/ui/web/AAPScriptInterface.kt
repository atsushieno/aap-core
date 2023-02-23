package org.androidaudioplugin.ui.web

import android.util.Log
import android.webkit.JavascriptInterface
import org.androidaudioplugin.ParameterInformation
import org.androidaudioplugin.PortInformation
import java.nio.ByteBuffer

/**
 * `AAPScriptInterface` is the basis for a reference implementation for the Web UI.
 * A WebView hosting reference implementation (AAPClientScriptInterface) and a WebView
 * native reference implementation (AAPServiceScriptInterface) make use of it.
 *
 * It should store the GUI events into the plugin's event input buffer, which should be -
 *
 * - "unified" with the audio plugin's process() event inputs, when the plugin is "activated"
 * - immediately processed when it is not at "activated" state.
 *
 * The AudioPluginInstance is responsible to store those event inputs and deal with above.
 */
@Suppress("unused")
abstract class AAPScriptInterface {

    private val AAP_LOG_TAG = "AAPScriptInterface"

    @JavascriptInterface
    fun log(s: String) {
        println(s)
    }

    @JavascriptInterface
    fun onInitialize() {
        Log.i(AAP_LOG_TAG, "onInitialize() invoked.")
    }

    @JavascriptInterface
    fun onCleanup() {
        Log.i(AAP_LOG_TAG, "onCleanup() invoked.")
    }

    @JavascriptInterface
    fun onNotify(port: Int, data: ByteArray, offset: Int, length: Int) {
        println("!!!!!!!!!!!!!!! onNotify")
    }

    @JavascriptInterface
    fun listen(port: Int) {
    }

    @JavascriptInterface
    abstract fun sendMidi1(data: ByteArray, offset: Int, length: Int)

    @JavascriptInterface
    abstract fun setParameter(parameterId: Int, value: Double)

    private val tmpBuffer: ByteBuffer = ByteBuffer.allocate(4)

    @JavascriptInterface
    abstract fun getPortBuffer(port: Int, data: ByteArray, length: Int)

    @get:JavascriptInterface
    abstract val portCount: Int

    @JavascriptInterface
    abstract fun getPort(index: Int) : JsPortInformation

    @get:JavascriptInterface
    abstract val parameterCount: Int

    @JavascriptInterface
    abstract fun getParameter(index: Int): JsParameterInformation


    class JsPortInformation(port: PortInformation) {
        @get:JavascriptInterface
        val index = port.index
        @get:JavascriptInterface
        val name = port.name
        @get:JavascriptInterface
        val content = port.content
        @get:JavascriptInterface
        val direction = port.direction
    }

    class JsParameterInformation(para: ParameterInformation) {
        @get:JavascriptInterface
        val id = para.id
        @get:JavascriptInterface
        val name = para.name
        @get:JavascriptInterface
        val minValue = para.minimumValue
        @get:JavascriptInterface
        val maxValue = para.maximumValue
        @get:JavascriptInterface
        val defaultValue = para.defaultValue
    }
}
