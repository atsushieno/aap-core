package org.androidaudioplugin.hosting

import android.app.Activity
import android.content.ComponentName
import android.content.Context
import android.content.Intent
import android.content.ServiceConnection
import android.os.Build
import android.os.IBinder
import android.os.RemoteException
import android.util.Log
import org.androidaudioplugin.AudioPluginService
import org.androidaudioplugin.AudioPluginException
import org.androidaudioplugin.AudioPluginNatives
import org.androidaudioplugin.PluginServiceInformation
import java.util.Collections
import java.util.concurrent.Executors
import kotlin.coroutines.resume
import kotlinx.coroutines.suspendCancellableCoroutine
import kotlinx.coroutines.withTimeoutOrNull

/*
  The flags to bind plugin services with. A plugin UI may need to start an activity, e.g. for a file
  chooser, while its process has no visible activity. Since Android 14 such a launch is only allowed
  if the visible host binds the plugin with BIND_ALLOW_ACTIVITY_STARTS.
 */
internal val pluginServiceBindFlags = Context.BIND_AUTO_CREATE or
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.UPSIDE_DOWN_CAKE) Context.BIND_ALLOW_ACTIVITY_STARTS else 0

/*
  Runs the ServiceConnection callbacks. It has no cap on threads, so a callback that blocks can
  never hold up another one it waits for. Running them directly on the binder thread that delivers
  them would use up the bounded binder pool, which plugins also need to call into the host.
 */
private val connectionCallbackExecutor = Executors.newCachedThreadPool {
    Thread(it, "aap-service-connection").apply { isDaemon = true }
}

/*
  A host client class that manages one or more connections to AudioPluginServices.

  Native hosts also use this class to instantiate plugins and manage them.

  Connection callbacks, and so the listeners and the native notification, run on any pool thread
  and never need the Android main thread, so a bind can be waited for on any thread. They may
  block. No order is promised, even within a connection: anything that needs one keeps its own queue.
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
            if (!parent.isBindingPending(this))
                return
            Log.d("AAP", "AudioPluginServiceConnector: onServiceConnected")
            val conn = if (binder == null) null else try {
                parent.registerNewConnection(this, serviceInfo, binder)
            } catch (ex: Exception) {
                Log.w("AAP", "AudioPluginServiceConnector: connection initialization failed", ex)
                null
            }
            // This completes the bind before the consumers get the connection. The bind deadline
            // is for the service to connect: if what they do with it counted, a slow consumer would
            // time out a good connection and take everything on that service down with it.
            onServiceConnectionRegistered(conn)
            if (conn == null)
                return
            // A concurrent successful bind may already own the service. Release only this attempt.
            if (conn.platformServiceConnection !== this)
                parent.releaseBinding(this)
            else
                parent.notifyConnected(conn)
        }

        override fun onServiceDisconnected(name: ComponentName?) {
            parent.failBinding(this, AudioPluginException("Plugin service disconnected: ${serviceInfo.packageName}/${serviceInfo.className}"))
        }

        override fun onNullBinding(name: ComponentName?) {
            onServiceConnectionRegistered(null)
        }

        override fun onBindingDied(name: ComponentName?) {
            parent.failBinding(this, AudioPluginException("Plugin service binding died: ${serviceInfo.packageName}/${serviceInfo.className}"))
        }
    }

    // A bind may complete or be closed before Context.bindService() returns. Defer unbinding
    // until that call returns, and retain the attempt's identity until its cleanup is claimed.
    private class PendingBinding(
        val key: PendingServiceConnection,
        var complete: ((Result<PluginServiceConnection>) -> Unit)?
    ) {
        var finished = false
        var bindStarted = false
        var bindReturned = false
        var releaseRequested = false
    }

    companion object {
        var serial = 0
    }

    var serviceConnectionId = serial++

    val connectedServices: MutableList<PluginServiceConnection> = Collections.synchronizedList(mutableListOf())
    private val connectionMutation = Any()
    private val pendingServices = mutableMapOf<PendingServiceConnection, PendingBinding>()
    private val bindings = mutableMapOf<ServiceConnection, PendingBinding>()

    val onConnectedListeners: MutableList<(PluginServiceConnection) -> Unit> = Collections.synchronizedList(mutableListOf())
    val onDisconnectingListeners: MutableList<(PluginServiceConnection) -> Unit> = Collections.synchronizedList(mutableListOf())

    @Volatile private var isClosed = false

    suspend fun bindAudioPluginService(service: PluginServiceInformation) = withTimeoutOrNull(10_000L) {
        suspendCancellableCoroutine<PluginServiceConnection> { continuation ->
            val pendingKey = PendingServiceConnection(service.packageName, service.className)
            val intent = Intent(AudioPluginHostHelper.AAP_ACTION_NAME)
            intent.component = ComponentName(service.packageName, service.className)
            val requestForeground = context is Activity
            intent.putExtra(AudioPluginService.EXTRA_REQUEST_FOREGROUND, requestForeground)

            lateinit var conn: Connection
            conn = Connection(this@AudioPluginServiceConnector, service) { result ->
                if (result == null)
                    failBinding(conn, AudioPluginException("Failed to bind AudioPluginService: Intent = $intent"))
                else
                    finishBinding(conn, Result.success(result))
            }
            val binding = PendingBinding(pendingKey) { continuation.resumeWith(it) }
            val existing = synchronized(connectionMutation) {
                if (isClosed)
                    throw AudioPluginException("AudioPluginServiceConnector is closed")
                val found = findExistingServiceConnection(service.packageName, service.className)
                if (found == null) {
                    if (pendingServices.containsKey(pendingKey))
                        throw AudioPluginException("AudioPluginService is already being bound: ${service.packageName}/${service.className}")
                    pendingServices[pendingKey] = binding
                    bindings[conn] = binding
                }
                found
            }
            if (existing != null) {
                continuation.resume(existing)
                return@suspendCancellableCoroutine
            }
            continuation.invokeOnCancellation { error ->
                // Cancelling a bind ends the wait for it, including when it lands after completion
                // but before this caller could run. A registered connection is not this caller's
                // to drop, as others may be using it already.
                if (findRegisteredConnection(conn) == null)
                    failBinding(conn, error ?: AudioPluginException("AudioPluginService binding cancelled"))
            }
            if (!isBindingPending(conn))
                return@suspendCancellableCoroutine

            try {
                if (requestForeground) {
                    try {
                        if (context.startForegroundService(intent) == null)
                            Log.w("AAP", "AudioPluginServiceConnector: startForegroundService returned null for $pendingKey")
                    } catch (ex: Exception) {
                        Log.w("AAP", "AudioPluginServiceConnector: startForegroundService failed for $pendingKey", ex)
                    }
                }
                if (!synchronized(connectionMutation) {
                        if (binding.releaseRequested) false else {
                            binding.bindStarted = true
                            true
                        }
                    })
                    return@suspendCancellableCoroutine
                if (!context.bindService(intent, pluginServiceBindFlags, connectionCallbackExecutor, conn))
                    failBinding(conn, AudioPluginException("AudioPluginServiceConnector: bindService returned false for $pendingKey"))
            } catch (ex: Exception) {
                failBinding(conn, ex)
            } finally {
                val release = synchronized(connectionMutation) {
                    binding.bindReturned = true
                    binding.releaseRequested
                }
                if (release)
                    releaseBinding(conn)
            }
        }
    } ?: findExistingServiceConnection(service.packageName, service.className) // it connected, but this caller could not run in time
        ?: throw AudioPluginException("Timed out binding AudioPluginService: ${service.packageName}/${service.className}")

    private fun isBindingPending(connection: ServiceConnection) = synchronized(connectionMutation) {
        bindings[connection]?.let { !it.finished && !it.releaseRequested } == true
    }

    private fun findRegisteredConnection(connection: ServiceConnection) = synchronized(connectionMutation) {
        connectedServices.toTypedArray().firstOrNull { it.platformServiceConnection === connection }
    }

    private fun finishBinding(connection: ServiceConnection, result: Result<PluginServiceConnection>) {
        val completion = synchronized(connectionMutation) {
            val binding = bindings[connection] ?: return
            if (binding.finished)
                return
            binding.finished = true
            if (pendingServices[binding.key] === binding)
                pendingServices.remove(binding.key)
            binding.complete.also { binding.complete = null }
        }
        completion?.invoke(result)
    }

    private fun failBinding(connection: ServiceConnection, error: Throwable) {
        val binding = synchronized(connectionMutation) {
            val found = bindings[connection] ?: return
            found.releaseRequested = true
            found.finished = true
            Pair(found, found.complete.also { found.complete = null })
        }
        try {
            val registered = findRegisteredConnection(connection)
            if (registered != null)
                invalidateConnection(registered.serviceInfo.packageName, registered.serviceInfo.className, registered.binder, connection)
        } finally {
            releaseBinding(connection)
            synchronized(connectionMutation) {
                if (pendingServices[binding.first.key] === binding.first)
                    pendingServices.remove(binding.first.key)
            }
            binding.second?.invoke(Result.failure(error))
        }
    }

    private fun releaseBinding(connection: ServiceConnection) {
        val unbind = synchronized(connectionMutation) {
            val binding = bindings[connection] ?: return
            binding.releaseRequested = true
            if (!binding.bindStarted) {
                bindings.remove(connection)
                false
            } else if (!binding.bindReturned) {
                false
            } else {
                bindings.remove(connection)
                true
            }
        }
        if (unbind)
            unbindPlatformConnection(connection)
    }

    private fun unbindPlatformConnection(connection: ServiceConnection) {
        try {
            context.unbindService(connection)
        } catch (ex: IllegalArgumentException) {
            Log.w("AAP", "AudioPluginServiceConnector: service already unbound", ex)
        }
    }

    private fun registerNewConnection(serviceConnection: ServiceConnection, serviceInfo: PluginServiceInformation, binder: IBinder) : PluginServiceConnection {
        return synchronized(connectionMutation) {
            if (isClosed || !isBindingPending(serviceConnection))
                throw AudioPluginException("AudioPluginService binding is no longer pending")
            val existing = findExistingServiceConnection(serviceInfo.packageName, serviceInfo.className)
            if (existing != null) {
                Log.w(
                    "AAP",
                    "AudioPluginServiceConnector: duplicate connection ignored for ${serviceInfo.packageName}/${serviceInfo.className}"
                )
                return existing
            }
            val deathRecipient = IBinder.DeathRecipient {
                Log.w(
                    "AAP",
                    "AudioPluginServiceConnector: binder died for ${serviceInfo.packageName}/${serviceInfo.className}"
                )
                failBinding(serviceConnection, AudioPluginException("Plugin service disconnected during binding"))
            }
            try {
                binder.linkToDeath(deathRecipient, 0)
            } catch (ex: RemoteException) {
                Log.w(
                    "AAP",
                    "AudioPluginServiceConnector: binder already dead for ${serviceInfo.packageName}/${serviceInfo.className}",
                    ex
                )
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
                throw ex
            }
            if (!binder.isBinderAlive || isClosed || !isBindingPending(serviceConnection)) {
                AudioPluginNatives.removeBinderForClient(serviceConnectionId, serviceInfo.packageName, serviceInfo.className)
                binder.unlinkToDeath(deathRecipient, 0)
                throw AudioPluginException("Plugin service disconnected during connection registration")
            }
            connectedServices.add(conn)
            conn
        }
    }

    private fun notifyConnected(conn: PluginServiceConnection) {
        // It can be gone already: the bind was cancelled or the service died right after registration.
        if (findExistingServiceConnection(conn.serviceInfo.packageName, conn.serviceInfo.className) !== conn)
            return
        // A failing consumer must not take the connection away from the others.
        try {
            nativeOnServiceConnectedCallback(conn.serviceInfo.packageName)
        } catch (ex: Exception) {
            Log.w("AAP", "AudioPluginServiceConnector: native connection callback failed", ex)
        }
        onConnectedListeners.toTypedArray().forEach {
            try {
                it(conn)
            } catch (ex: Exception) {
                Log.w("AAP", "AudioPluginServiceConnector: connection listener failed", ex)
            }
        }
    }

    fun invalidateServiceConnection(packageName: String, className: String? = null): PluginServiceConnection? =
        invalidateConnection(packageName, className, null)

    private fun invalidateConnection(packageName: String, className: String?, expectedBinder: IBinder?, expectedConnection: ServiceConnection? = null): PluginServiceConnection? {
        val (conn, managedBinding) = synchronized(connectionMutation) {
            val found = findExistingServiceConnection(packageName, className) ?: return null
            if ((expectedBinder != null && found.binder !== expectedBinder) ||
                (expectedConnection != null && found.platformServiceConnection !== expectedConnection))
                return null
            connectedServices.remove(found)
            NativeParameterMetadata.invalidateConnection(serviceConnectionId, found.serviceInfo.packageName, found.serviceInfo.className)
            AudioPluginNatives.removeBinderForClient(
                serviceConnectionId, found.serviceInfo.packageName, found.serviceInfo.className
            )
            Pair(found, bindings.containsKey(found.platformServiceConnection))
        }
        try {
            finishBinding(conn.platformServiceConnection, Result.failure(AudioPluginException("Plugin service disconnected before binding completed")))
            onDisconnectingListeners.toTypedArray().forEach { it(conn) }
        } finally {
            conn.deathRecipient?.let {
                try {
                    conn.binder.unlinkToDeath(it, 0)
                } catch (_: NoSuchElementException) {
                    // already gone
                } catch (_: Throwable) {
                    // binder is already dead or detached
                }
            }
            if (managedBinding)
                releaseBinding(conn.platformServiceConnection)
            else
                unbindPlatformConnection(conn.platformServiceConnection)
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
            Pair(bindings.keys.toTypedArray(), connectedServices.toTypedArray())
        }
        snapshot.first.forEach { connection ->
            failBinding(connection, AudioPluginException("AudioPluginServiceConnector is closed"))
        }
        snapshot.second.forEach { conn ->
            invalidateConnection(conn.serviceInfo.packageName, conn.serviceInfo.className, conn.binder, conn.platformServiceConnection)
        }
    }
}
