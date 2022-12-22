package org.androidaudioplugin.hosting

import android.content.ComponentName
import android.content.Context
import android.content.Intent
import android.content.ServiceConnection
import android.os.IBinder
import android.util.Log
import org.androidaudioplugin.AudioPluginNatives
import org.androidaudioplugin.PluginServiceInformation
import kotlin.coroutines.resume
import kotlin.coroutines.suspendCoroutine

/*
  A host client class that manages one or more connections to AudioPluginServices.

  Native hosts also use this class to instantiate plugins and manage them.
 */
class AudioPluginServiceConnector(val applicationContext: Context) : AutoCloseable {
    /*
    The ServiceConnection implementation class for AudioPluginService.
     */
    internal class Connection(private val parent: AudioPluginServiceConnector, private val serviceInfo: PluginServiceInformation, private val additionalOnServiceConnected: (PluginServiceConnection?) -> Unit) : ServiceConnection {

        override fun onServiceConnected(name: ComponentName?, binder: IBinder?) {
            Log.d("AAP", "AudioPluginServiceConnector: onServiceConnected")
            if (binder != null)
                additionalOnServiceConnected(parent.registerNewConnection(this, serviceInfo, binder))
        }

        override fun onServiceDisconnected(name: ComponentName?) {
            Log.d("AAP", "AudioPluginServiceConnector: onServiceDisconnected - FIXME: implement")
            parent.unbindAudioPluginService(serviceInfo.packageName)
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

    var instanceId = serial++

    val connectedServices = mutableListOf<PluginServiceConnection>()

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
                // FIXME: use dedicated Exception type
                throw Exception("Failed to bind AudioPluginService: Intent = $intent")
        }

        Log.d(
            "AudioPluginHost",
            "bindAudioPluginService: ${service.packageName} | ${service.className}"
        )
        assert(applicationContext.bindService(intent, conn, Context.BIND_AUTO_CREATE))
    }

    private fun registerNewConnection(serviceConnection: ServiceConnection, serviceInfo: PluginServiceInformation, binder: IBinder) : PluginServiceConnection {
        val conn = PluginServiceConnection(serviceConnection, serviceInfo, binder)
        AudioPluginNatives.addBinderForClient(
            instanceId,
            conn.serviceInfo.packageName,
            conn.serviceInfo.className,
            conn.binder!!
        )
        connectedServices.add(conn)

        nativeOnServiceConnectedCallback(conn.serviceInfo.packageName)
        return conn
    }

    private external fun nativeOnServiceConnectedCallback(servicePackageName: String)

    fun findExistingServiceConnection(packageName: String) =
        connectedServices.firstOrNull { conn -> conn.serviceInfo.packageName == packageName }

    fun unbindAudioPluginService(packageName: String) {
        val conn = findExistingServiceConnection(packageName) ?: return
        connectedServices.remove(conn)
        AudioPluginNatives.removeBinderForHost(
            instanceId,
            conn.serviceInfo.packageName,
            conn.serviceInfo.className
        )
        applicationContext.unbindService(conn.platformServiceConnection)
    }

    override fun close() {
        connectedServices.toTypedArray().forEach { conn ->
            AudioPluginNatives.removeBinderForHost(
                instanceId,
                conn.serviceInfo.packageName,
                conn.serviceInfo.className
            )
        }
        isClosed = true
    }
}