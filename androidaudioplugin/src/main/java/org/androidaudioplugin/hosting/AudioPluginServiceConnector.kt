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
import java.util.Collections
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
        @Volatile private var registeredBinder: IBinder? = null

        override fun onServiceConnected(name: ComponentName?, binder: IBinder?) {
            Log.d("AAP", "AudioPluginServiceConnector: onServiceConnected")
            if (binder != null) {
                registeredBinder = binder
                val conn = try {
                    parent.registerNewConnection(this, serviceInfo, binder)
                } catch (ex: Exception) {
                    Log.w("AAP", "AudioPluginServiceConnector: connection initialization failed", ex)
                    try {
                        parent.context.unbindService(this)
                    } catch (_: IllegalArgumentException) {
                    }
                    null
                }
                onServiceConnectionRegistered(conn)
            } else {
                onServiceConnectionRegistered(null)
            }
        }

        override fun onServiceDisconnected(name: ComponentName?) {
            Log.w("AAP", "AudioPluginServiceConnector: onServiceDisconnected for ${serviceInfo.packageName}/${serviceInfo.className}")
            parent.invalidateConnection(serviceInfo.packageName, serviceInfo.className, registeredBinder)
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
            parent.invalidateConnection(serviceInfo.packageName, serviceInfo.className, registeredBinder)
        }
    }

    companion object {
        var serial = 0
    }

    var serviceConnectionId = serial++

    val connectedServices: MutableList<PluginServiceConnection> = Collections.synchronizedList(mutableListOf())
    private val connectionMutation = Any()
    private val pendingServices = mutableSetOf<PendingServiceConnection>()

    val onConnectedListeners: MutableList<(PluginServiceConnection) -> Unit> = Collections.synchronizedList(mutableListOf())
    val onDisconnectingListeners: MutableList<(PluginServiceConnection) -> Unit> = Collections.synchronizedList(mutableListOf())

    @Volatile private var isClosed = false

    suspend fun bindAudioPluginService(service: PluginServiceInformation) = suspendCoroutine { continuation ->
        if (isClosed)
            throw AudioPluginException("AudioPluginServiceConnector is closed")

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
        val conn = synchronized(connectionMutation) {
            if (isClosed)
                throw AudioPluginException("AudioPluginServiceConnector is closed")
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
                invalidateConnection(serviceInfo.packageName, serviceInfo.className, binder)
            }
            try {
                binder.linkToDeath(deathRecipient, 0)
            } catch (ex: RemoteException) {
                Log.w(
                    "AAP",
                    "AudioPluginServiceConnector: binder already dead for ${serviceInfo.packageName}/${serviceInfo.className}",
                    ex
                )
                try {
                    context.unbindService(serviceConnection)
                } catch (_: IllegalArgumentException) {
                }
                throw AudioPluginException("Plugin service disconnected before connection registration", ex)
            }
            val conn = PluginServiceConnection(serviceConnection, serviceInfo, binder, deathRecipient)
            // A Java IBinder object created by Service framework is converted to NdkBinder object here.
            // It must happen somewhere within `ServiceConnection.onServiceConnected()`.
            try {
                AudioPluginNatives.addBinderForClient(
                    serviceConnectionId,
                    conn.serviceInfo.packageName,
                    conn.serviceInfo.className,
                    conn.binder
                )
            } catch (ex: Exception) {
                binder.unlinkToDeath(deathRecipient, 0)
                try {
                    context.unbindService(serviceConnection)
                } catch (_: IllegalArgumentException) {
                }
                throw ex
            }
            if (!binder.isBinderAlive) {
                AudioPluginNatives.removeBinderForClient(serviceConnectionId, serviceInfo.packageName, serviceInfo.className)
                binder.unlinkToDeath(deathRecipient, 0)
                try {
                    context.unbindService(serviceConnection)
                } catch (_: IllegalArgumentException) {
                }
                throw AudioPluginException("Plugin service disconnected during connection registration")
            }
            connectedServices.add(conn)
            conn
        }

        nativeOnServiceConnectedCallback(conn.serviceInfo.packageName)

        onConnectedListeners.toTypedArray().forEach { it(conn) }

        return conn
    }

    fun invalidateServiceConnection(packageName: String, className: String? = null): PluginServiceConnection? =
        invalidateConnection(packageName, className, null)

    private fun invalidateConnection(packageName: String, className: String?, expectedBinder: IBinder?): PluginServiceConnection? {
        val conn = synchronized(connectionMutation) {
            val found = findExistingServiceConnection(packageName, className) ?: return null
            if (expectedBinder != null && found.binder !== expectedBinder)
                return null
            connectedServices.remove(found)
            AudioPluginNatives.removeBinderForClient(
                serviceConnectionId, found.serviceInfo.packageName, found.serviceInfo.className
            )
            found
        }
        onDisconnectingListeners.toTypedArray().forEach { it(conn) }

        conn.deathRecipient?.let {
            try {
                conn.binder.unlinkToDeath(it, 0)
            } catch (_: NoSuchElementException) {
                // already gone
            } catch (_: Throwable) {
                // binder is already dead or detached
            }
        }
        try {
            context.unbindService(conn.platformServiceConnection)
        } catch (ex: IllegalArgumentException) {
            Log.w("AAP", "AudioPluginServiceConnector: service already unbound for ${conn.serviceInfo.packageName}/${conn.serviceInfo.className}", ex)
        }
        return conn
    }

    private external fun nativeOnServiceConnectedCallback(servicePackageName: String)

    fun findExistingServiceConnection(packageName: String, className: String? = null) =
        synchronized(connectedServices) {
            connectedServices.firstOrNull { conn ->
                conn.serviceInfo.packageName == packageName &&
                    (className == null || conn.serviceInfo.className == className)
            }
        }

    fun unbindAudioPluginService(packageName: String) {
        invalidateServiceConnection(packageName)
    }

    override fun close() {
        val snapshot = synchronized(connectionMutation) {
            if (isClosed)
                return
            isClosed = true
            connectedServices.toTypedArray()
        }
        snapshot.forEach { conn ->
            invalidateConnection(conn.serviceInfo.packageName, conn.serviceInfo.className, conn.binder)
        }
    }
}
