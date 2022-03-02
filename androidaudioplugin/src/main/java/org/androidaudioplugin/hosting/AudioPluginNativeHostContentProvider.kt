package org.androidaudioplugin.hosting

import android.content.ContentProvider
import android.content.ContentValues
import android.content.Context
import android.content.pm.ProviderInfo
import android.database.Cursor
import android.net.Uri
import org.androidaudioplugin.AudioPluginHost
import org.androidaudioplugin.AudioPluginNatives
import org.androidaudioplugin.PluginServiceInformation
import java.lang.RuntimeException

// This class can be used to initialize AudioPluginHost by specifying the class as a <provider>
// element in  AndroidManifest.xml, then AudioPluginHost can automatically initializes the required
// audio plugin list for local setup.
class AudioPluginNativeHostContentProvider : ContentProvider()
{
    override fun insert(uri: Uri, values: ContentValues?): Uri? {
        throw RuntimeException("not supported")
    }

    override fun query(uri: Uri, projection: Array<out String>?, selection: String?,
                       selectionArgs: Array<out String>?, sortOrder: String?): Cursor? {
        throw RuntimeException("not supported")
    }

    override fun onCreate(): Boolean {
        System.loadLibrary("androidaudioplugin")
        return true
    }

    override fun attachInfo(context: Context?, info: ProviderInfo?) {
        AudioPluginNatives.initializeAAPJni(context!!)
        val host = AudioPluginHost(context)
        // FIXME: it is brutal, but is the only way for fully-native JUCE apps to prepare
        // audio plugin service connections beforehand so far.
        val services = AudioPluginHostHelper.queryAudioPluginServices(context)
        for (service in services) {
            host.serviceConnector.bindAudioPluginService(
                PluginServiceInformation(
                    service.label,
                    service.packageName,
                    service.className
                ),
                host.sampleRate)
        }
        // FIXME: it should add callback to wait for bindService() result otherwise app don't
        //  actually get those services in the end.
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