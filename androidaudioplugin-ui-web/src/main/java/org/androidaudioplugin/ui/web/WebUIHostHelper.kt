package org.androidaudioplugin.ui.web

import android.annotation.SuppressLint
import android.content.Context
import android.webkit.MimeTypeMap
import android.webkit.WebResourceRequest
import android.webkit.WebResourceResponse
import android.webkit.WebView
import android.webkit.WebViewClient
import androidx.webkit.WebViewAssetLoader
import kotlinx.coroutines.DelicateCoroutinesApi
import kotlinx.coroutines.GlobalScope
import kotlinx.coroutines.launch
import org.androidaudioplugin.hosting.AudioPluginHostHelper
import org.androidaudioplugin.hosting.AudioPluginInstance
import java.io.ByteArrayInputStream
import java.io.ByteArrayOutputStream
import java.util.concurrent.Semaphore
import java.util.zip.ZipInputStream

// Asynchronous version of ZipArchivePathHandler that waits for asynchronous content resolution from plugin.
@OptIn(DelicateCoroutinesApi::class)
class LazyZipArchivePathHandler(context: Context, instance: AudioPluginInstance) : WebViewAssetLoader.PathHandler {
    private val semaphore = Semaphore(1)
    lateinit var resolver: ZipArchivePathHandler
    override fun handle(path: String): WebResourceResponse? {
        // wait for zip resource acquisition.
        try {
            semaphore.acquire()
            return resolver.handle(path)
        } finally {
            semaphore.release()
        }
    }

    init {
        semaphore.acquire()
        GlobalScope.launch {
            val bytes = WebUIHostHelper.retrieveWebUIArchive(
                context,
                instance.pluginInfo.pluginId!!,
                instance.pluginInfo.packageName
            )
                ?: throw IllegalStateException("The requested Web UI archive could not be retrieved.")
            resolver = ZipArchivePathHandler(bytes)
            semaphore.release()
        }
    }
}

// WebView Asset PathHandler that returns contents of a zip archive. Synchronous version.
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

object WebUIHostHelper {
    suspend fun retrieveWebUIArchive(context: Context, pluginId: String, packageName: String? = null) : ByteArray? {
        // FIXME:
        // Right now it does not make a lot of sense because WebUIHelper is in the same library,
        // but it will be implemented to request AAP services to send their UI resource archive.
        if (false) {
            val pluginInfo =
                (if (packageName != null) AudioPluginHostHelper.queryAudioPluginService(
                    context,
                    packageName
                ).plugins.firstOrNull { it.pluginId == pluginId }
                else AudioPluginHostHelper.queryAudioPlugins(context)
                    .firstOrNull { it.pluginId == pluginId }) ?: return null
            TODO("Not implemented")
        } else
            return WebUIHelper.getDefaultUIZipArchive(context, pluginId)
    }

    @SuppressLint("SetJavaScriptEnabled")
    fun getWebView(
        ctx: Context,
        instance: AudioPluginInstance,
        supportInProcessAssets: Boolean = false
    ): WebView {

        return WebView(ctx).also { webView ->
            val builder = WebViewAssetLoader.Builder()
            if (supportInProcessAssets)
            // it is used for local assets, meaning that it is not for remote plugin process.
                builder.addPathHandler("/assets/", WebViewAssetLoader.AssetsPathHandler(ctx))
            val assetLoader =
                // it is used for the remote zip archive. It is then asynchronously loaded.
                builder.addPathHandler("/zip/", LazyZipArchivePathHandler(ctx, instance))
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
            webView.addJavascriptInterface(AAPScriptInterface(instance), "AAPInterop")

            webView.loadUrl("https://appassets.androidplatform.net/zip/index.html")
        }
    }
}
