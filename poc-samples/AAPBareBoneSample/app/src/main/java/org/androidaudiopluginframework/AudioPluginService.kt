package org.androidaudiopluginframework

import android.app.Notification
import android.app.Service
import android.content.Intent
import android.os.IBinder
import android.util.Log
import androidx.core.app.NotificationCompat

open class AudioPluginService : Service()
{
    companion object {
        const val NATIVE_LIBRARY_NAME = "aapnativebridge"
        const val AAP_ACTION_NAME = "org.androidaudiopluginframework.AudioPluginService"

        init {
            System.loadLibrary(NATIVE_LIBRARY_NAME)
        }

        @JvmStatic
        external fun createBinder() : IBinder
    }

    var native_binder : IBinder? = null

    override fun onBind(intent: Intent?): IBinder? {
        Log.d ("AudioPluginService", "onBind invoked")
        if (native_binder == null)
            native_binder = createBinder()
        return native_binder
    }

    override fun onCreate() {
        super.onCreate()
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        Log.d("AudioPluginService", "onStartCommand invoked")

        val notification = NotificationCompat.Builder(this, "audiopluginservice-" + hashCode())
            .setContentTitle("Foreground Service")
            .setContentText("started FG service")
            .build()
        this.startForeground(1, notification)

        return super.onStartCommand(intent, flags, startId)
    }
}
