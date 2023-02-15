package org.androidaudioplugin.ui.web

import android.annotation.SuppressLint
import android.content.Context
import android.webkit.WebResourceRequest
import android.webkit.WebResourceResponse
import android.webkit.WebView
import android.webkit.WebViewClient
import androidx.webkit.WebViewAssetLoader
import org.androidaudioplugin.AudioPluginException
import org.androidaudioplugin.AudioPluginViewFactory
import org.androidaudioplugin.AudioPluginServiceHelper
import org.androidaudioplugin.NativeLocalPluginInstance
import org.androidaudioplugin.NativePluginService

class AudioPluginWebViewFactory : AudioPluginViewFactory() {
    override fun createView(context: Context, pluginId: String, instanceId: Int) =
        getWebView(context, pluginId, instanceId)

    @SuppressLint("SetJavaScriptEnabled")
    fun getWebView(ctx: Context, pluginId: String, instanceId: Int): WebView {
        val pluginInfo = AudioPluginServiceHelper.getLocalAudioPluginService(ctx).plugins.firstOrNull { it.pluginId == pluginId }
            ?: throw AudioPluginException("Plugin '$pluginId' not found.")
        return WebView(ctx).also { webView ->
            val builder = WebViewAssetLoader.Builder()
            // it is used for local assets, meaning that it is not for remote plugin process.
            builder.addPathHandler("/assets/", WebViewAssetLoader.AssetsPathHandler(ctx))
            val assetLoader =
                // it is used for the remote zip archive. It is then asynchronously loaded.
                builder.addPathHandler("/zip/", LazyZipArchivePathHandler(
                    ctx, pluginId, pluginInfo.packageName))
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
            val nativeInstance = NativePluginService(pluginId).getInstance(instanceId)
            webView.addJavascriptInterface(AAPServiceScriptInterface(nativeInstance), "AAPInterop")

            webView.loadUrl("https://appassets.androidplatform.net/zip/index.html")
        }
    }
}