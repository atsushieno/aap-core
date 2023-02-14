package org.androidaudioplugin

import android.app.Service
import android.content.Intent
import android.os.IBinder
import android.util.Log
import androidx.core.app.NotificationCompat
import android.app.NotificationManager
import android.app.NotificationChannel
import android.content.Context

/**
 * The Audio plugin service class. It should not be derived. We check the service class name strictly.
 *
 * Every AAP should implement AudioPluginInterface.aidl, in native code.
 *
 * Typical user developers don't have to write any code to create their own plugins in Kotlin code.
 * They are implemented in native code to get closest to realtime audio processing.
 */
open class AudioPluginService : Service()
{
    companion object {
        private var initialized = false
        private var waitForDebugger = false

        fun initialize(context: Context) {
            if (initialized)
                return
            if (waitForDebugger) {
                android.os.Debug.waitForDebugger()
                waitForDebugger = true
            }
            AudioPluginNatives.initializeAAPJni(context)
            initialized = true
        }
    }

    /**
     * This interface is used by AAP Service extensions that will be initialized at instantiation step.
     * Not to be confused with AAP extension in the native (audio plugin) API context (`get_extension()`).
     *
     * By registering this extension as a `<meta-data>` element in AndroidManifest.xml, whose
     * `android:name` is `org.androidaudioplugin.AudioPluginService#Extensions`, the implementation
     * class that is specified in the `android:value` attribute of the `<meta-data>` element
     * will be instantiated and those members are invoked, wherever appropriate.
     */
    interface Extension {
        fun initialize(ctx: Context)
        fun cleanup()
    }

    private var nativeBinder : IBinder? = null
    var extensions = mutableListOf<Extension>()

    override fun onCreate() {
        initialize(this)
    }

    override fun onBind(intent: Intent?): IBinder? {
        val existing = nativeBinder
        if (existing != null)
            AudioPluginNatives.destroyBinderForService(existing)
        nativeBinder = AudioPluginNatives.createBinderForService()

        val si = AudioPluginServiceHelper.getLocalAudioPluginService(this)
        si.extensions.forEach { e ->
            if(e == "")
                return@forEach
            val c = Class.forName(e)
            val ext = c.newInstance() as Extension
            ext.initialize(this)
            extensions.add(ext)
        }
        return nativeBinder
    }

    override fun onUnbind(intent: Intent?): Boolean {
        for(ext in extensions)
            ext.cleanup()

        val binder = nativeBinder
        if (binder != null)
            AudioPluginNatives.destroyBinderForService(binder)
        nativeBinder = null
        return true
    }
}
