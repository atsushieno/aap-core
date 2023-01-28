package org.androidaudioplugin

import android.app.Service
import android.content.Intent
import android.os.IBinder
import android.util.Log
import androidx.core.app.NotificationCompat
import android.app.NotificationManager
import android.app.NotificationChannel
import android.content.Context
import android.os.Build
import org.androidaudioplugin.hosting.AudioPluginMidiSettings

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

    companion object {
        /**
         * Enable it only if you're debugging. Otherwise it might suspend if the debuggee is a different process.
         *
         * Also, if it is true, the shutdown process by lack of active sensing messages should be disabled.
         */
        @JvmStatic
        var enableDebug = false
    }

    private var native_binder : IBinder? = null

    override fun onBind(intent: Intent?): IBinder? {
        if (enableDebug)
            android.os.Debug.waitForDebugger()
        AudioPluginLocalHost.initialize(this)
        val existing = native_binder
        if (existing != null)
            AudioPluginNatives.destroyBinderForService(existing)
        native_binder = AudioPluginNatives.createBinderForService()
        Log.d("AudioPluginService", "onBind done");
        return native_binder
    }

    override fun onUnbind(intent: Intent?): Boolean {
        AudioPluginLocalHost.cleanup()
        val binder = native_binder
        if (binder != null)
            AudioPluginNatives.destroyBinderForService(binder)
        native_binder = null
        return true
    }

    // FIXME: we should probably just remove them, we're not foreground service now (still unsure for future).
    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        val channelID = "AndroidAudioPluginServiceChannel"

        val serviceChannel = NotificationChannel(
            channelID,
            "AAP Foreground Service Channel",
            NotificationManager.IMPORTANCE_LOW
        )

        val manager = getSystemService(NotificationManager::class.java)
        manager!!.createNotificationChannel(serviceChannel)

        val notification = NotificationCompat.Builder(this, channelID)
            .setContentTitle("Foreground Service")
            .setContentText("started FG service")
            .build()
        this.startForeground(startId, notification)

        return START_NOT_STICKY
    }
}
