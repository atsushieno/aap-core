package org.androidaudioplugin.ui.web

import android.annotation.SuppressLint
import android.content.Context
import android.webkit.JavascriptInterface
import android.webkit.MimeTypeMap
import android.webkit.WebResourceRequest
import android.webkit.WebResourceResponse
import android.webkit.WebView
import android.webkit.WebViewClient
import androidx.annotation.RequiresApi
import androidx.webkit.WebViewAssetLoader
import org.androidaudioplugin.PluginInformation
import org.androidaudioplugin.hosting.AudioPluginInstance
import java.io.ByteArrayInputStream
import java.io.ByteArrayOutputStream
import java.nio.ByteBuffer
import java.util.zip.ZipInputStream

// WebView Asset PathHandler that returns contents of a zip archive.
class ZipArchivePathHandler(private val zipArchiveBytes: ByteArray) : WebViewAssetLoader.PathHandler {

    override fun handle(path: String): WebResourceResponse? {
        val bs = ByteArrayInputStream(zipArchiveBytes)
        val zs = ZipInputStream(bs)
        while(true) {
            val entry = zs.nextEntry ?: break
            if (entry.name == path) {
                if (entry.size > Int.MAX_VALUE)
                    return WebResourceResponse("text/html", "utf-8", 500, "Internal Error", mapOf(), ByteArrayInputStream("Internal Error: compressed zip entry ${entry.name} has bloated size: ${entry.size}".toByteArray()))
                // huh, entry.size is not really available??
                val os = ByteArrayOutputStream(4096)
                val bytes = ByteArray(4096)
                while (true) {
                    val size = zs.read(bytes, 0, bytes.size)
                    if (size < 0)
                        break
                    os.write(bytes, 0, size)
                }
                os.close()
                val ext = MimeTypeMap.getFileExtensionFromUrl(path)
                return WebResourceResponse(
                    MimeTypeMap.getSingleton().getMimeTypeFromExtension(ext),
                    "utf-8",
                    ByteArrayInputStream(os.toByteArray()))
            }
        }
        return WebResourceResponse("text/html", "utf-8", 404, "Not Found", mapOf(), ByteArrayInputStream("Not found".toByteArray()))
    }
}

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
        fun writeMidi(data: ByteArray) {
            println("!!!!!!!!!!!!!!! writeMidi(${data[0]}, ${data[1]}, ${data[2]})")
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

        // Right now it does not make a lot of sense because WebUIHelper is in the same library,
        // but it will be implemented to request AAP services to send their UI resource archive.
        private fun getWebUIArchive(ctx: Context, pluginId: String) : ByteArray {
            return WebUIHelper.getUIZipArchive(ctx, pluginId)
        }

        @SuppressLint("SetJavaScriptEnabled")
        fun getWebView(ctx: Context, plugin: AudioPluginInstance) : WebView {

            val bytes = getWebUIArchive(ctx, plugin.pluginInfo.pluginId!!)

            return WebView(ctx).also { webView ->
                val assetLoader = WebViewAssetLoader.Builder()
                    .addPathHandler("/assets/", WebViewAssetLoader.AssetsPathHandler(ctx))
                    .addPathHandler("/zip/", ZipArchivePathHandler(bytes))
                    .build()
                webView.webViewClient = object : WebViewClient() {
                    override fun shouldInterceptRequest(
                        view: WebView,
                        request: WebResourceRequest
                    ): WebResourceResponse? {
                        return assetLoader.shouldInterceptRequest(request.url)
                    }
                }
                webView.settings.javaScriptEnabled = true
                webView.addJavascriptInterface(AAPScriptInterface(), "AAPInterop")

                webView.loadUrl("https://appassets.androidplatform.net/zip/index.html")
            }
        }
    }
}
