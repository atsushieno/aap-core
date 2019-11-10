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

open class AudioPluginService : Service()
{
    companion object {
        const val NATIVE_LIBRARY_NAME = "androidaudioplugin"

        init {
            System.loadLibrary(NATIVE_LIBRARY_NAME)
        }

        @JvmStatic
        external fun createBinder(sampleRate: Int) : IBinder

        @JvmStatic
        external fun destroyBinder(binder: IBinder)
    }
    var native_binder : IBinder? = null

    override fun onBind(intent: Intent?): IBinder? {
        android.os.Debug.waitForDebugger()
        val sampleRate = intent!!.getIntExtra("sampleRate", 44100)
        AudioPluginHost.initialize(this)
        if (native_binder == null)
            native_binder = createBinder(sampleRate)
        return native_binder
    }

    override fun onUnbind(intent: Intent?): Boolean {
        AudioPluginHost.cleanup()
        if (native_binder != null)
            destroyBinder(native_binder!!)
        return true
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        val channelID = "AndroidAudioPluginServiceChannel"

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            val serviceChannel = NotificationChannel(
                channelID,
                "AAP Foreground Service Channel",
                NotificationManager.IMPORTANCE_LOW
            )

            val manager = getSystemService(NotificationManager::class.java)
            manager!!.createNotificationChannel(serviceChannel)
        }

        val notification = NotificationCompat.Builder(this, channelID)
            .setContentTitle("Foreground Service")
            .setContentText("started FG service")
            .build()
        this.startForeground(startId, notification)

        return START_NOT_STICKY
    }
}
