package org.androidaudioplugin.midideviceservice

import android.annotation.SuppressLint
import android.content.Context
import android.webkit.*
import androidx.annotation.RequiresApi
import androidx.webkit.WebViewAssetLoader
import org.androidaudioplugin.PluginInformation
import java.nio.ByteBuffer

class WebUIHostHelper {

    private class AAPScriptInterface {
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
        fun write(port: Int, value: String) {
            tmpBuffer.asFloatBuffer().put(0, value.toFloat())
            write(port, tmpBuffer.array(), 0, 4)
        }
        private val tmpBuffer: ByteBuffer = ByteBuffer.allocate(4)

        @JavascriptInterface
        fun write(port: Int, data: ByteArray, offset: Int, length: Int) {
            println("!!!!!!!!!!!!!!! write($port, ${ByteBuffer.wrap(data).asFloatBuffer().get(offset)})")
        }
    }

    companion object {
        @SuppressLint("SetJavaScriptEnabled")
        fun getWebView(ctx: Context, plugin: PluginInformation) =
            WebView(ctx).also { webView ->
                val assetLoader = WebViewAssetLoader.Builder().addPathHandler(
                    "/assets/",
                    WebViewAssetLoader.AssetsPathHandler(ctx)
                ).build()
                webView.webViewClient = object : WebViewClient() {
                    @RequiresApi(21)
                    override fun shouldInterceptRequest(
                        view: WebView,
                        request: WebResourceRequest
                    ): WebResourceResponse? {
                        return assetLoader.shouldInterceptRequest(request.url)
                    }
                }
                webView.settings.javaScriptEnabled = true
                webView.addJavascriptInterface(AAPScriptInterface(), "AAPInterop")

                webView.loadData(WebUIHelper.getHtml(plugin), "text/html", null)
            }
    }
}