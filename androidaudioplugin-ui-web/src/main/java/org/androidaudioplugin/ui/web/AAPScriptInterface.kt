package org.androidaudioplugin.ui.web

import android.webkit.JavascriptInterface
import org.androidaudioplugin.ParameterInformation
import org.androidaudioplugin.PortInformation
import org.androidaudioplugin.hosting.AudioPluginInstance
import java.nio.ByteBuffer

@Suppress("unused")
class AAPScriptInterface(val instance: AudioPluginInstance) {
    @JavascriptInterface
    fun log(s: String) {
        println(s)
    }

    @JavascriptInterface
    fun onInitialize() {
        println("!!!!!!!!!!!!!!! onInitialize")
    }

    @JavascriptInterface
    fun onCleanup() {
        println("!!!!!!!!!!!!!!! onCleanup")
    }

    @JavascriptInterface
    fun onShow() {
        println("!!!!!!!!!!!!!!! onShow")
    }

    @JavascriptInterface
    fun onHide() {
        println("!!!!!!!!!!!!!!! onHide")
    }

    @JavascriptInterface
    fun onNotify(port: Int, data: ByteArray, offset: Int, length: Int) {
        println("!!!!!!!!!!!!!!! onNotify")
    }

    @JavascriptInterface
    fun listen(port: Int) {
    }

    @JavascriptInterface
    fun writeMidi(data: ByteArray) {
        println("!!!!!!!!!!!!!!! writeMidi(${data[0]}, ${data[1]}, ${data[2]})")
    }

    @JavascriptInterface
    fun setParameter(parameterId: Int, value: Double) {
        println("!!!!!!!!!!!!!!! setParameter($parameterId, $value)")
    }
    private val tmpBuffer: ByteBuffer = ByteBuffer.allocate(4)

    @JavascriptInterface
    fun write(port: Int, data: ByteArray, offset: Int, length: Int) {
        println("!!!!!!!!!!!!!!! write($port, ${ByteBuffer.wrap(data).asFloatBuffer().get(offset)})")
    }

    @get:JavascriptInterface
    val portCount: Int
        get() = instance.getPortCount()
    @JavascriptInterface
    fun getPort(index: Int) = JsPortInformation(instance.getPort(index))
    @get:JavascriptInterface
    val parameterCount: Int
        get() = instance.getParameterCount()
    @JavascriptInterface
    fun getParameter(id: Int) = JsParameterInformation(instance.getParameter(id))

    class JsPortInformation(val port: PortInformation) {
        @get:JavascriptInterface
        val index: Int
            get() = port.index
        @get:JavascriptInterface
        val name: String
            get() = port.name
        @get:JavascriptInterface
        val content: Int
            get() = port.content
        @get:JavascriptInterface
        val direction: Int
            get() = port.direction
    }

    class JsParameterInformation(val para: ParameterInformation) {
        @JavascriptInterface
        fun getId() = para.id
        @JavascriptInterface
        fun getName() = para.name
        @JavascriptInterface
        fun getMinValue() = para.minimumValue
        @JavascriptInterface
        fun getMaxValue() = para.maximumValue
        @JavascriptInterface
        fun getDefaultValue() = para.defaultValue
    }
}
