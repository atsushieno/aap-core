package org.androidaudioplugin.hosting

import android.app.Activity
import android.content.ComponentName
import android.content.Context
import android.content.Intent
import android.content.ServiceConnection
import android.os.IBinder
import android.util.Log
import org.androidaudioplugin.AudioPluginException
import org.androidaudioplugin.AudioPluginNatives
import org.androidaudioplugin.AudioPluginService
import org.androidaudioplugin.AudioPluginServiceHelper
import org.androidaudioplugin.PluginServiceInformation
import kotlin.coroutines.resume
import kotlin.coroutines.suspendCoroutine

/*
  A host client class that manages one or more connections to AudioPluginServices.

  Native hosts also use this class to instantiate plugins and manage them.
 */
class AudioPluginServiceConnector(val context: Context) : AutoCloseable {
    private data class PendingServiceConnection(
        val packageName: String,
        val className: String
    )

    /*
    The ServiceConnection implementation class for AudioPluginService.
     */
    internal class Connection(private val parent: AudioPluginServiceConnector, private val serviceInfo: PluginServiceInformation, private val onServiceConnectionRegistered: (PluginServiceConnection?) -> Unit) : ServiceConnection {

        override fun onServiceConnected(name: ComponentName?, binder: IBinder?) {
            Log.d("AAP", "AudioPluginServiceConnector: onServiceConnected")
            if (binder != null) {
                val conn = parent.registerNewConnection(this, serviceInfo, binder)
                onServiceConnectionRegistered(conn)
            }
        }

        override fun onServiceDisconnected(name: ComponentName?) {
            Log.d("AAP", "AudioPluginServiceConnector: onServiceDisconnected - FIXME: implement")
        }

        override fun onNullBinding(name: ComponentName?) {
            Log.d("AAP", "AudioPluginServiceConnector: onNullBinding")
            super.onNullBinding(name)
        }

        override fun onBindingDied(name: ComponentName?) {
            Log.d("AAP", "AudioPluginServiceConnector: onBindingDied")
            super.onBindingDied(name)
        }
    }

    companion object {
        var serial = 0
    }

    var serviceConnectionId = serial++

    val connectedServices = mutableListOf<PluginServiceConnection>()
    private val pendingServices = mutableSetOf<PendingServiceConnection>()

    val onConnectedListeners = mutableListOf<(PluginServiceConnection) -> Unit>()
    val onDisconnectingListeners = mutableListOf<(PluginServiceConnection) -> Unit>()

    private var isClosed = false

    suspend fun bindAudioPluginService(service: PluginServiceInformation) = suspendCoroutine { continuation ->
        assert(!isClosed)

        val pendingKey = PendingServiceConnection(service.packageName, service.className)
        val existing = findExistingServiceConnection(service.packageName, service.className)
        if (existing != null) {
            continuation.resume(existing)
            return@suspendCoroutine
        }
        synchronized(pendingServices) {
            if (!pendingServices.add(pendingKey)) {
                continuation.resumeWith(Result.failure(AudioPluginException(
                    "AudioPluginService is already being bound: ${service.packageName}/${service.className}"
                )))
                return@suspendCoroutine
            }
        }

        val intent = Intent(AudioPluginHostHelper.AAP_ACTION_NAME)
        intent.component = ComponentName(
            service.packageName,
            service.className
        )

        val conn = Connection(this, service) { pluginServiceConnection ->
            synchronized(pendingServices) {
                pendingServices.remove(pendingKey)
            }
            if (pluginServiceConnection != null)
                continuation.resume(pluginServiceConnection)
            else
                continuation.resumeWith(Result.failure(AudioPluginException("Failed to bind AudioPluginService: Intent = $intent")))
        }

        Log.d(
            "AudioPluginHost",
            "bindAudioPluginService: ${service.packageName} | ${service.className}"
        )
        try {
            // start as FGS only if it is applicable
            if (context is Activity && context.startForegroundService(intent) == null) {
                val error = "AudioPluginServiceConnector: startForegroundService returned null for ${service.packageName}/${service.className}"
                Log.e("AAP", error)
                synchronized(pendingServices) {
                    pendingServices.remove(pendingKey)
                }
                continuation.resumeWith(Result.failure(AudioPluginException(error)))
                return@suspendCoroutine
            }
        } catch (ex: Throwable) {
            Log.e("AAP", "AudioPluginServiceConnector: startForegroundService failed for ${service.packageName}/${service.className}", ex)
            synchronized(pendingServices) {
                pendingServices.remove(pendingKey)
            }
            continuation.resumeWith(Result.failure(ex))
            return@suspendCoroutine
        }

        if (context !is Activity)
            Log.d("AAP", "AudioPluginServiceConnector: not as foreground service - context is not Activity.")

        try {
            if (!context.bindService(intent, conn, Context.BIND_AUTO_CREATE)) {
                val error = "AudioPluginServiceConnector: bindService returned false for ${service.packageName}/${service.className}"
                Log.e("AAP", error)
                synchronized(pendingServices) {
                    pendingServices.remove(pendingKey)
                }
                continuation.resumeWith(Result.failure(AudioPluginException(error)))
                return@suspendCoroutine
            }
        } catch (ex: Throwable) {
            Log.e("AAP", "AudioPluginServiceConnector: bindService threw for ${service.packageName}/${service.className}", ex)
            synchronized(pendingServices) {
                pendingServices.remove(pendingKey)
            }
            continuation.resumeWith(Result.failure(ex))
        }
    }

    private fun registerNewConnection(serviceConnection: ServiceConnection, serviceInfo: PluginServiceInformation, binder: IBinder) : PluginServiceConnection {
        val existing = findExistingServiceConnection(serviceInfo.packageName, serviceInfo.className)
        if (existing != null) {
            Log.w(
                "AAP",
                "AudioPluginServiceConnector: duplicate connection ignored for ${serviceInfo.packageName}/${serviceInfo.className}"
            )
            context.unbindService(serviceConnection)
            return existing
        }
        val conn = PluginServiceConnection(serviceConnection, serviceInfo, binder)
        // A Java IBinder object created by Service framework is converted to NdkBinder object here.
        // It must happen somewhere within `ServiceConnection.onServiceConnected()`.
        AudioPluginNatives.addBinderForClient(
            serviceConnectionId,
            conn.serviceInfo.packageName,
            conn.serviceInfo.className,
            conn.binder
        )

        connectedServices.add(conn)

        nativeOnServiceConnectedCallback(conn.serviceInfo.packageName)

        onConnectedListeners.forEach { it(conn) }

        return conn
    }

    private external fun nativeOnServiceConnectedCallback(servicePackageName: String)

    fun findExistingServiceConnection(packageName: String, className: String? = null) =
        connectedServices.firstOrNull { conn ->
            conn.serviceInfo.packageName == packageName &&
                (className == null || conn.serviceInfo.className == className)
        }

    fun unbindAudioPluginService(packageName: String) {
        val conn = findExistingServiceConnection(packageName) ?: return

        onDisconnectingListeners.forEach { it(conn) }

        connectedServices.remove(conn)
        AudioPluginNatives.removeBinderForClient(
            serviceConnectionId,
            conn.serviceInfo.packageName,
            conn.serviceInfo.className
        )
        context.unbindService(conn.platformServiceConnection)
    }

    override fun close() {
        connectedServices.toTypedArray().forEach { conn ->
            AudioPluginNatives.removeBinderForClient(
                serviceConnectionId,
                conn.serviceInfo.packageName,
                conn.serviceInfo.className
            )
        }
        isClosed = true
    }
}
