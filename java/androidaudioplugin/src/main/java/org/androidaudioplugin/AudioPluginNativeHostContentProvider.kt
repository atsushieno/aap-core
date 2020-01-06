package org.androidaudioplugin

import android.content.ContentProvider
import android.content.ContentValues
import android.content.Context
import android.content.pm.ProviderInfo
import android.database.Cursor
import android.net.Uri
import dalvik.system.DexClassLoader
import java.lang.RuntimeException

// This class can be used to initialize AudioPluginHost by specifying the class as a <provider>
// element in  AndroidManifest.xml, then AudioPluginHost can automatically initializes the required
// audio plugin list for local setup.
class AudioPluginNativeHostContentProvider : ContentProvider()
{
    companion object {
        const val NATIVE_LIBRARY_NAME = "androidaudioplugin"

        init {
            System.loadLibrary(NATIVE_LIBRARY_NAME)
        }
    }

    override fun insert(uri: Uri, values: ContentValues?): Uri? {
        throw RuntimeException("not supported")
    }

    override fun query(uri: Uri, projection: Array<out String>?, selection: String?,
                       selectionArgs: Array<out String>?, sortOrder: String?): Cursor? {
        throw RuntimeException("not supported")
    }

    override fun onCreate(): Boolean {
        return true
    }

    override fun attachInfo(context: Context?, info: ProviderInfo?) {
        AudioPluginHost.setApplicationContext(context!!)
        AudioPluginHost.initialize(AudioPluginHostHelper.queryAudioPlugins(context))
    }

    override fun update(
        uri: Uri,
        values: ContentValues?,
        selection: String?,
        selectionArgs: Array<out String>?
    ): Int {
        throw RuntimeException("not supported")
    }

    override fun delete(uri: Uri, selection: String?, selectionArgs: Array<out String>?): Int {
        throw RuntimeException("not supported")
    }

    override fun getType(uri: Uri): String? {
        throw RuntimeException("not supported")
    }
}