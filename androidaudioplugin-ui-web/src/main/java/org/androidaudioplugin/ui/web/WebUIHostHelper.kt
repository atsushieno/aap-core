package org.androidaudioplugin.ui.web

import android.annotation.SuppressLint
import android.content.Context
import android.net.Uri
import android.webkit.WebResourceRequest
import android.webkit.WebResourceResponse
import android.webkit.WebSettings
import android.webkit.WebView
import android.webkit.WebViewClient
import androidx.webkit.WebViewAssetLoader
import org.androidaudioplugin.hosting.AudioPluginHostHelper
import org.androidaudioplugin.hosting.AudioPluginInstance
import org.androidaudioplugin.hosting.NativeRemotePluginInstance
import java.io.FileInputStream
import java.util.concurrent.Semaphore

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

    fun getWebView(ctx: Context, instance: AudioPluginInstance, supportInProcessAssets: Boolean = false)
        = getWebView(ctx, instance.pluginInfo.pluginId!!, instance.pluginInfo.packageName, instance.native, supportInProcessAssets)

    @SuppressLint("SetJavaScriptEnabled")
    @JvmStatic
    fun getWebView(
        ctx: Context,
        pluginId: String,
        packageName: String,
        native: NativeRemotePluginInstance,
        supportInProcessAssets: Boolean = false
    ): WebView {
        return WebView(ctx).also { webView ->
            val builder = WebViewAssetLoader.Builder()
            if (supportInProcessAssets)
            // it is used for local assets, meaning that it is not for remote plugin process.
                builder.addPathHandler("/assets/", WebViewAssetLoader.AssetsPathHandler(ctx))
            val assetLoader =
                // it is used for the remote zip archive. It is then asynchronously loaded.
                builder.addPathHandler("/zip/", LazyZipArchivePathHandler(
                    ctx, pluginId, packageName))
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
            webView.settings.cacheMode = WebSettings.LOAD_NO_CACHE
            webView.clearCache(true)
            webView.addJavascriptInterface(AAPClientScriptInterface(native), "AAPInterop")

            webView.loadUrl("https://appassets.androidplatform.net/zip/index.html")
        }
    }
}
