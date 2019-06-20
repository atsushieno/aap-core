package org.androidaudiopluginframework

import android.app.Notification
import android.app.Service
import android.content.Intent
import android.os.IBinder
import android.util.Log
import android.R
import android.app.PendingIntent
import androidx.core.app.NotificationCompat
import android.app.NotificationManager
import android.app.NotificationChannel
import android.os.Build
import org.androidaudiopluginframework.samples.aaphostsample.MainActivity

open class AudioPluginService : Service()
{
    companion object {
        const val NATIVE_LIBRARY_NAME = "androidaudioplugin"
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

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        val channelID = "AndroidAudioPluginServiceChannel"

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            val serviceChannel = NotificationChannel(
                channelID,
                "Foreground Service Channel",
                NotificationManager.IMPORTANCE_DEFAULT
            )

            val manager = getSystemService(NotificationManager::class.java)
            manager!!.createNotificationChannel(serviceChannel)
        }

        val notification = NotificationCompat.Builder(this, channelID)
            .setContentTitle("Foreground Service")
            .setContentText("started FG service")
            .build()
        this.startForeground(startId, notification)

        return START_STICKY
    }
}
