package org.androidaudioplugin.ui.web

import android.content.ContentProvider
import android.content.ContentValues
import android.content.Context
import android.content.Intent
import android.database.Cursor
import android.net.Uri
import android.os.ParcelFileDescriptor
import java.io.ByteArrayOutputStream
import java.io.File
import java.io.FileOutputStream
import java.lang.IllegalStateException
import java.util.zip.ZipEntry
import java.util.zip.ZipOutputStream

object WebUIHelper {

    fun getDefaultUIZipArchive(ctx: Context, pluginId: String?) : ByteArray {
        val ms = ByteArrayOutputStream()
        val os = ZipOutputStream(ms)
        os.writer().use {w ->
            for (s in arrayOf("index.html", "webcomponents-lite.js", "webaudio-controls.js", "bright_life.png")) {
                os.putNextEntry(ZipEntry(s))
                ctx.assets.open("web/$s").use {
                    it.copyTo(os)
                }
                w.flush()
            }
        }
        return ms.toByteArray()
    }
}

class WebUIArchiveContentProvider : ContentProvider() {
    companion object {
        const val zipFileName = "web-ui.zip"
    }

    override fun onCreate(): Boolean = true

    override fun query(
        uri: Uri,
        projection: Array<out String>?,
        selection: String?,
        selectionArgs: Array<out String>?,
        sortOrder: String?
    ): Cursor? = null

    override fun getType(uri: Uri): String? = "application/zip"

    override fun insert(uri: Uri, values: ContentValues?): Uri? = null

    override fun delete(uri: Uri, selection: String?, selectionArgs: Array<out String>?): Int = 0

    override fun update(
        uri: Uri,
        values: ContentValues?,
        selection: String?,
        selectionArgs: Array<out String>?
    ): Int = 0

    override fun openFile(uri: Uri, mode: String): ParcelFileDescriptor? {
        val ctx = context ?: throw IllegalStateException("Android context is not available.")

        val providerUri = "content://${ctx.packageName}.aap_zip_provider"
        val contentUri = Uri.parse("$providerUri/org.androidaudioplugin.ui.web/web-ui.zip")
        ctx.grantUriPermission(callingPackage, contentUri, Intent.FLAG_GRANT_READ_URI_PERMISSION)

        val pluginId = uri.getQueryParameter("pluginId")

        if (!ctx.fileList().contains(zipFileName)) {
            // FIXME: we should support per-pluginId UI.
            FileOutputStream(File(ctx.filesDir, zipFileName)).use {
                // If there is "web-ui.zip" asset at the top level in the app, we send it.
                // Otherwise, send the default ui.
                if (ctx.assets.list(".")?.contains(zipFileName) == true)
                    ctx.assets.open(zipFileName).copyTo(it)
                else
                    it.write(WebUIHelper.getDefaultUIZipArchive(ctx, pluginId))
            }
        }
        return ParcelFileDescriptor.open(File(ctx.filesDir, zipFileName), ParcelFileDescriptor.MODE_READ_ONLY)
    }
}
