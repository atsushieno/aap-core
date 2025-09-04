package org.androidaudioplugin

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.Service
import android.content.ComponentName
import android.content.Context
import android.content.Intent
import android.os.IBinder
import android.os.Looper
import androidx.core.app.NotificationCompat

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
     * This interface is used by AAP Service extensions that will be initialized at onCreate().
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
    private val notificationManager by lazy { getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager }
    private val NOTIFICATION_ID = 1
    private val CHANNEL_ID by lazy { applicationInfo.packageName + ":" + javaClass.name }

    override fun onCreate() {
        initialize(this)
        
        createNotificationChannel()

        val si = AudioPluginServiceHelper.getLocalAudioPluginService(this)
        si.extensions.forEach { e ->
            if(e == "")
                return@forEach
            val c = Class.forName(e)
            val ext = c.newInstance() as Extension
            ext.initialize(this)
            extensions.add(ext)
        }
    }

    private var eventProcessorLooper: Looper? = null

    // Depending on the plugin, it may be missing foreground service type in the manifest.
    // To avoid crash due to insufficient permission, we make foreground behavior optional.
    private val declaredForegroundServiceType by lazy { packageManager.getServiceInfo(ComponentName(applicationContext, javaClass.name), 0).foregroundServiceType }

    override fun onBind(intent: Intent?): IBinder? {
        val existing = nativeBinder
        if (existing != null)
            AudioPluginNatives.destroyBinderForService(existing)
        nativeBinder = AudioPluginNatives.createBinderForService()

        // no need to worry about the Looper retaining; it will be released at onUnbind()
        Thread {
            AudioPluginNatives.prepareNativeLooper()
            eventProcessorLooper = Looper.myLooper()
            AudioPluginNatives.startNativeLooper()
        }.apply {
            this.contextClassLoader = contextClassLoader
            this.start()
        }

        if (declaredForegroundServiceType != 0)
            startForegroundService()

        return nativeBinder
    }

    override fun onUnbind(intent: Intent?): Boolean {
        for(ext in extensions)
            ext.cleanup()

        AudioPluginNatives.stopNativeLooper()
        if (eventProcessorLooper?.thread?.isAlive == true)
            eventProcessorLooper?.quitSafely()
        eventProcessorLooper = null

        val binder = nativeBinder
        if (binder != null)
            AudioPluginNatives.destroyBinderForService(binder)
        nativeBinder = null

        if (declaredForegroundServiceType != 0)
            stopForeground(STOP_FOREGROUND_REMOVE)
        
        return true
    }
    
    private fun createNotificationChannel() {
        val name = applicationInfo.packageName
        val descriptionText = getString(R.string.audio_plugin_service_notification_channel_description)
        val importance = NotificationManager.IMPORTANCE_LOW
        val channel = NotificationChannel(CHANNEL_ID, name, importance).apply {
            description = descriptionText
        }
        notificationManager.createNotificationChannel(channel)
    }
    
    private fun startForegroundService() {
        val notification = createNotification()
        startForeground(NOTIFICATION_ID, notification, declaredForegroundServiceType)
    }
    
    private fun createNotification(): Notification {
        return NotificationCompat.Builder(this, CHANNEL_ID)
            // It may be confusing if there are more than one AudioPluginService...
            .setContentTitle("AAP: " + applicationInfo.packageName)
            .setContentText(getString(R.string.audio_plugin_service_notification_text))
            // FIXME: this should become customizable and identifiable
            .setSmallIcon(android.R.drawable.ic_media_play)
            .setOngoing(true)
            .build()
    }
}
