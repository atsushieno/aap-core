package org.androidaudioplugin.ui.web

import android.webkit.JavascriptInterface
import org.androidaudioplugin.ParameterInformation
import org.androidaudioplugin.PortInformation
import java.nio.ByteBuffer

@Suppress("unused")
abstract class AAPScriptInterface {
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
        val buf = ByteBuffer.wrap(data, offset, length).asFloatBuffer().get(offset)
        println("!!!!!!!!!!!!!!! write($port, ($buf), $offset, $length)")
    }

    @get:JavascriptInterface
    abstract val portCount: Int

    @JavascriptInterface
    abstract fun getPort(index: Int) : JsPortInformation

    @get:JavascriptInterface
    abstract val parameterCount: Int

    abstract fun getParameter(id: Int): JsParameterInformation

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

