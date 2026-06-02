package org.androidaudioplugin.hosting

import android.app.Activity
import android.content.ComponentName
import android.content.Context
import android.content.Intent
import android.content.ServiceConnection
import android.os.IBinder
import android.os.RemoteException
import android.util.Log
import org.androidaudioplugin.AudioPluginService
import org.androidaudioplugin.AudioPluginException
import org.androidaudioplugin.AudioPluginNatives
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
            } else {
                onServiceConnectionRegistered(null)
            }
        }

        override fun onServiceDisconnected(name: ComponentName?) {
            Log.w("AAP", "AudioPluginServiceConnector: onServiceDisconnected for ${serviceInfo.packageName}/${serviceInfo.className}")
            parent.invalidateServiceConnection(serviceInfo.packageName, serviceInfo.className)
        }

        override fun onNullBinding(name: ComponentName?) {
            Log.w("AAP", "AudioPluginServiceConnector: onNullBinding for ${serviceInfo.packageName}/${serviceInfo.className}")
            synchronized(parent.pendingServices) {
                parent.pendingServices.remove(PendingServiceConnection(serviceInfo.packageName, serviceInfo.className))
            }
            onServiceConnectionRegistered(null)
        }

        override fun onBindingDied(name: ComponentName?) {
            Log.w("AAP", "AudioPluginServiceConnector: onBindingDied for ${serviceInfo.packageName}/${serviceInfo.className}")
            parent.invalidateServiceConnection(serviceInfo.packageName, serviceInfo.className)
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
        val requestForeground = context is Activity
        intent.putExtra(AudioPluginService.EXTRA_REQUEST_FOREGROUND, requestForeground)

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
        if (requestForeground) {
            try {
                val started = context.startForegroundService(intent)
                if (started == null) {
                    Log.w("AAP", "AudioPluginServiceConnector: startForegroundService returned null for ${service.packageName}/${service.className}")
                }
            } catch (ex: Throwable) {
                Log.w("AAP", "AudioPluginServiceConnector: startForegroundService failed for ${service.packageName}/${service.className}", ex)
            }
        }
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
            try {
                context.unbindService(serviceConnection)
            } catch (ex: IllegalArgumentException) {
                Log.w("AAP", "AudioPluginServiceConnector: duplicate connection was already unbound", ex)
            }
            return existing
        }
        val deathRecipient = IBinder.DeathRecipient {
            Log.w(
                "AAP",
                "AudioPluginServiceConnector: binder died for ${serviceInfo.packageName}/${serviceInfo.className}"
            )
            invalidateServiceConnection(serviceInfo.packageName, serviceInfo.className)
        }
        try {
            binder.linkToDeath(deathRecipient, 0)
        } catch (ex: RemoteException) {
            Log.w(
                "AAP",
                "AudioPluginServiceConnector: binder already dead for ${serviceInfo.packageName}/${serviceInfo.className}",
                ex
            )
            return invalidateServiceConnection(serviceInfo.packageName, serviceInfo.className)
                ?: PluginServiceConnection(serviceConnection, serviceInfo, binder)
        }
        val conn = PluginServiceConnection(serviceConnection, serviceInfo, binder, deathRecipient)
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

    fun invalidateServiceConnection(packageName: String, className: String? = null): PluginServiceConnection? {
        val conn = findExistingServiceConnection(packageName, className) ?: return null

        onDisconnectingListeners.forEach { it(conn) }

        connectedServices.remove(conn)
        conn.deathRecipient?.let {
            try {
                conn.binder.unlinkToDeath(it, 0)
            } catch (_: NoSuchElementException) {
                // already gone
            } catch (_: Throwable) {
                // binder is already dead or detached
            }
        }
        AudioPluginNatives.removeBinderForClient(
            serviceConnectionId,
            conn.serviceInfo.packageName,
            conn.serviceInfo.className
        )
        try {
            context.unbindService(conn.platformServiceConnection)
        } catch (ex: IllegalArgumentException) {
            Log.w("AAP", "AudioPluginServiceConnector: service already unbound for ${conn.serviceInfo.packageName}/${conn.serviceInfo.className}", ex)
        }
        return conn
    }

    private external fun nativeOnServiceConnectedCallback(servicePackageName: String)

    fun findExistingServiceConnection(packageName: String, className: String? = null) =
        connectedServices.firstOrNull { conn ->
            conn.serviceInfo.packageName == packageName &&
                (className == null || conn.serviceInfo.className == className)
        }

    fun unbindAudioPluginService(packageName: String) {
        invalidateServiceConnection(packageName)
    }

    override fun close() {
        connectedServices.toTypedArray().forEach { conn ->
            conn.deathRecipient?.let {
                try {
                    conn.binder.unlinkToDeath(it, 0)
                } catch (_: NoSuchElementException) {
                } catch (_: Throwable) {
                }
            }
            AudioPluginNatives.removeBinderForClient(
                serviceConnectionId,
                conn.serviceInfo.packageName,
                conn.serviceInfo.className
            )
        }
        isClosed = true
    }
}
