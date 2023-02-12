package org.androidaudioplugin.ui.web

import android.annotation.SuppressLint
import android.content.Context
import android.net.Uri
import android.webkit.MimeTypeMap
import android.webkit.WebResourceRequest
import android.webkit.WebResourceResponse
import android.webkit.WebView
import android.webkit.WebViewClient
import androidx.webkit.WebViewAssetLoader
import org.androidaudioplugin.hosting.AudioPluginHostHelper
import org.androidaudioplugin.hosting.AudioPluginInstance
import java.io.ByteArrayInputStream
import java.io.ByteArrayOutputStream
import java.io.FileInputStream
import java.util.concurrent.Semaphore
import java.util.zip.ZipInputStream

// Asynchronous version of ZipArchivePathHandler that waits for asynchronous content resolution from plugin.
// NOTE: it is meant to be asynchronous, but so far use of content resolver is all synchronous operation (should not be harmful though).
class LazyZipArchivePathHandler(context: Context, instance: AudioPluginInstance) : WebViewAssetLoader.PathHandler {
    private val semaphore = Semaphore(1)
    private var resolver: ZipArchivePathHandler? = null
    override fun handle(path: String): WebResourceResponse? {
        // wait for zip resource acquisition.
        try {
            semaphore.acquire()
            return resolver?.handle(path)
        } finally {
            semaphore.release()
        }
    }

    init {
        semaphore.acquire()
        val bytes = WebUIHostHelper.retrieveWebUIArchive(
            context,
            instance.pluginInfo.pluginId!!,
            instance.pluginInfo.packageName
        )
        if (bytes != null)
            resolver = ZipArchivePathHandler(bytes)
        semaphore.release()
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
    fun retrieveWebUIArchive(context: Context, pluginId: String, packageName: String? = null) : ByteArray? {
        val pluginInfo =
            (if (packageName != null) AudioPluginHostHelper.queryAudioPluginService(
                context,
                packageName
            ).plugins.firstOrNull { it.pluginId == pluginId }
            else AudioPluginHostHelper.queryAudioPlugins(context)
                .firstOrNull { it.pluginId == pluginId }) ?: return null

        val providerUri = "content://${pluginInfo.packageName}.aap_zip_provider"
        val uri = Uri.parse("$providerUri/org.androidaudioplugin.ui.web/web-ui.zip")
        context.contentResolver.acquireContentProviderClient(uri). use {
            // It does not offer the Web UI
            if (it == null)
                return null
        }
        context.contentResolver.openFile(uri, "r", null).use {
            // The Content Provider somehow does not serve the zip resource under the URI above.
            if (it == null)
                return null
            FileInputStream(it.fileDescriptor).use { stream -> return stream.readBytes() }
        }
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
