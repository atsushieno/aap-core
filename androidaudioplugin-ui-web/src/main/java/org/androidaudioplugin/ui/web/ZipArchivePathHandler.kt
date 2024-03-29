package org.androidaudioplugin.ui.web

import android.content.Context
import android.webkit.MimeTypeMap
import android.webkit.WebResourceResponse
import androidx.webkit.WebViewAssetLoader
import java.io.ByteArrayInputStream
import java.io.ByteArrayOutputStream
import java.util.concurrent.Semaphore
import java.util.zip.ZipInputStream

// WebView Asset PathHandler that returns contents of a zip archive. Synchronous version.
class ZipArchivePathHandler(private val zipArchiveBytes: ByteArray) :
    WebViewAssetLoader.PathHandler {

    override fun handle(path: String): WebResourceResponse? {
        val bs = ByteArrayInputStream(zipArchiveBytes)
        val zs = ZipInputStream(bs)
        while(true) {
            val entry = zs.nextEntry ?: break
            if (entry.name == path) {
                if (entry.size > Int.MAX_VALUE)
                    return WebResourceResponse(
                        "text/html",
                        "utf-8",
                        500,
                        "Internal Error",
                        mapOf(),
                        ByteArrayInputStream("Internal Error: compressed zip entry ${entry.name} has bloated size: ${entry.size}".toByteArray())
                    )
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
                    ByteArrayInputStream(os.toByteArray())
                )
            }
        }
        return WebResourceResponse(
            "text/html",
            "utf-8",
            404,
            "Not Found",
            mapOf(),
            ByteArrayInputStream("Not found".toByteArray())
        )
    }
}

// Asynchronous version of ZipArchivePathHandler that waits for asynchronous content resolution from plugin.
// NOTE: it is meant to be asynchronous, but so far use of content resolver is all synchronous operation (should not be harmful though).
class LazyZipArchivePathHandler(context: Context, pluginId: String, packageName: String) : WebViewAssetLoader.PathHandler {
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
            pluginId,
            packageName
        )
        if (bytes != null)
            resolver = ZipArchivePathHandler(bytes)
        semaphore.release()
    }
}
