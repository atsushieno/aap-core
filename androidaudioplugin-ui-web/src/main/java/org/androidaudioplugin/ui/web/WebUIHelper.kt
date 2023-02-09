package org.androidaudioplugin.ui.web

import android.content.Context
import java.io.ByteArrayOutputStream
import java.io.InputStreamReader
import java.util.zip.ZipEntry
import java.util.zip.ZipOutputStream

object WebUIHelper {

    fun getDefaultUIZipArchive(ctx: Context, pluginId: String) : ByteArray {
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

