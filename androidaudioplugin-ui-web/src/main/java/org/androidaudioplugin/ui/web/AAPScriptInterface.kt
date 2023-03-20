package org.androidaudioplugin.ui.web

import android.util.Log
import android.webkit.JavascriptInterface
import dev.atsushieno.ktmidi.Midi1ToUmpTranslatorContext
import dev.atsushieno.ktmidi.Midi2Music
import dev.atsushieno.ktmidi.Midi2Track
import dev.atsushieno.ktmidi.Ump
import dev.atsushieno.ktmidi.UmpRetriever
import dev.atsushieno.ktmidi.UmpTranslator
import dev.atsushieno.ktmidi.toPlatformBytes
import dev.atsushieno.ktmidi.toPlatformNativeBytes
import org.androidaudioplugin.ParameterInformation
import org.androidaudioplugin.PortInformation
import org.androidaudioplugin.hosting.UmpHelper
import java.nio.ByteBuffer
import java.nio.ByteOrder

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

    private val tmpBuffer: ByteBuffer = ByteBuffer.allocateDirect(16)

    @JavascriptInterface
    open fun sendMidi1(data: ByteArray, offset: Int, length: Int) {
        val ctx = Midi1ToUmpTranslatorContext(data.toList(), group = 0)
        UmpTranslator.translateMidi1BytesToUmp(ctx)
        tmpBuffer.position(0)
        var size = 0
        ctx.output.map { it.toPlatformNativeBytes() }.forEach {
            tmpBuffer.put(it)
            size += it.size
        }
        addEventUmpInput(tmpBuffer, size)
    }

    @JavascriptInterface
    open fun setParameter(parameterId: Int, value: Double) {
        val s8 = UmpHelper.aapUmpSysex8Parameter(parameterId.toUInt(), value.toFloat())
        tmpBuffer.position(0)
        tmpBuffer.order(ByteOrder.nativeOrder())
        tmpBuffer.asIntBuffer().put(s8.toIntArray())
        addEventUmpInput(tmpBuffer, s8.size * Int.SIZE_BYTES)
    }

    // Passes the UMP inputs to the bound plugin instance.
    // Note that the input UMP must be in *native* order.
    abstract fun addEventUmpInput(data: ByteBuffer, size: Int)

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
