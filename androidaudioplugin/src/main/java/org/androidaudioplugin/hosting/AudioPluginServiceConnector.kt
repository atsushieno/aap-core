package org.androidaudioplugin.hosting

import android.content.ComponentName
import android.content.Context
import android.content.Intent
import android.util.Log
import org.androidaudioplugin.AudioPluginNatives
import org.androidaudioplugin.PluginServiceInformation

/*
  Manages one or more connections to AudioPluginServices.

  A plugin host instance holds an instance of this class.

 */
class AudioPluginServiceConnector(private val applicationContext: Context) : AutoCloseable {
    companion object {
        var serial = 0
    }

    var instanceId = serial++

    val serviceConnectedListeners = mutableListOf<(conn: PluginServiceConnection) -> Unit>()

    val connectedServices = mutableListOf<PluginServiceConnection>()

    private var isClosed = false

    fun bindAudioPluginService(service: PluginServiceInformation) {
        assert(!isClosed)

        val intent = Intent(AudioPluginHostHelper.AAP_ACTION_NAME)
        intent.component = ComponentName(
            service.packageName,
            service.className
        )

        val conn = PluginServiceConnection(service) { c -> onBindAudioPluginService(c) }

        Log.d(
            "AudioPluginHost",
            "bindAudioPluginService: ${service.packageName} | ${service.className}"
        )
        applicationContext.bindService(intent, conn, Context.BIND_AUTO_CREATE)
    }

    private fun onBindAudioPluginService(conn: PluginServiceConnection) {
        AudioPluginNatives.addBinderForClient(
            instanceId,
            conn.serviceInfo.packageName,
            conn.serviceInfo.className,
            conn.binder!!
        )
        connectedServices.add(conn)

        // avoid conflicting concurrent updates
        val currentListeners = serviceConnectedListeners.toTypedArray()
        currentListeners.forEach { f -> f(conn) }
    }

    fun findExistingServiceConnection(packageName: String, className: String) =
        connectedServices.firstOrNull { conn -> conn.serviceInfo.packageName == packageName && conn.serviceInfo.className == className }

    fun unbindAudioPluginService(packageName: String, localName: String) {
        val conn = findExistingServiceConnection(packageName, localName) ?: return
        connectedServices.remove(conn)
        AudioPluginNatives.removeBinderForHost(
            instanceId,
            conn.serviceInfo.packageName,
            conn.serviceInfo.className
        )
    }

    override fun close() {
        connectedServices.toTypedArray().forEach { conn ->
            AudioPluginNatives.removeBinderForHost(
                instanceId,
                conn.serviceInfo.packageName,
                conn.serviceInfo.className
            )
        }
        serviceConnectedListeners.clear()
        isClosed = true
    }
}