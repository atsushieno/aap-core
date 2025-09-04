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

    val onConnectedListeners = mutableListOf<(PluginServiceConnection) -> Unit>()
    val onDisconnectingListeners = mutableListOf<(PluginServiceConnection) -> Unit>()

    private var isClosed = false

    suspend fun bindAudioPluginService(service: PluginServiceInformation) = suspendCoroutine { continuation ->
        assert(!isClosed)

        val intent = Intent(AudioPluginHostHelper.AAP_ACTION_NAME)
        intent.component = ComponentName(
            service.packageName,
            service.className
        )

        val conn = Connection(this, service) { pluginServiceConnection ->
            if (pluginServiceConnection != null)
                continuation.resume(pluginServiceConnection)
            else
                continuation.resumeWith(Result.failure(AudioPluginException("Failed to bind AudioPluginService: Intent = $intent")))
        }

        Log.d(
            "AudioPluginHost",
            "bindAudioPluginService: ${service.packageName} | ${service.className}"
        )
        // start as FGS only if it is applicable
        if (context is Activity)
            assert(context.startForegroundService(intent) != null)
        else
            Log.d("AAP", "AudioPluginServiceConnector: not as foreground service - context is not Activity.")
        assert(context.bindService(intent, conn, Context.BIND_AUTO_CREATE))
    }

    private fun registerNewConnection(serviceConnection: ServiceConnection, serviceInfo: PluginServiceInformation, binder: IBinder) : PluginServiceConnection {
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

    fun findExistingServiceConnection(packageName: String) =
        connectedServices.firstOrNull { conn -> conn.serviceInfo.packageName == packageName && conn.serviceInfo.className == AudioPluginService::class.qualifiedName }

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