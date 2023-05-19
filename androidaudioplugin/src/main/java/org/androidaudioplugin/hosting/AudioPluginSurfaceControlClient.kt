package org.androidaudioplugin.hosting

import android.content.ComponentName
import android.content.Context
import android.content.Intent
import android.content.ServiceConnection
import android.os.Build
import android.os.Handler
import android.os.HandlerThread
import android.os.IBinder
import android.os.Looper
import android.os.Message
import android.os.Messenger
import android.view.SurfaceControlViewHost
import android.view.SurfaceView
import android.view.ViewGroup
import androidx.annotation.RequiresApi
import androidx.core.os.bundleOf
import org.androidaudioplugin.AudioPluginViewService
import kotlin.coroutines.resume
import kotlin.coroutines.suspendCoroutine

class AudioPluginSurfaceControlClient(private val context: Context) {
    private val messageHandlerThread = HandlerThread("IncomingMessengerHandler").apply { start() }
    @RequiresApi(Build.VERSION_CODES.R)
    private val incomingMessenger = Messenger(ClientReplyHandler(messageHandlerThread.looper) {
        view.setChildSurfacePackage(it)
        println("client: surfaceView.setChildSurfacePackage() done.")
    })

    private val view = SurfaceView(context).apply { setZOrderOnTop(true) }

    val surfaceView = view

    @RequiresApi(Build.VERSION_CODES.R)
    suspend fun connectUI(instance: AudioPluginInstance, width: Int, height: Int) {
        val pluginPackageName = instance.pluginInfo.packageName
        val pluginId = instance.pluginInfo.pluginId
        val instanceId = instance.instanceId

        view.apply {
            context.mainLooper.queue.addIdleHandler {
                layoutParams.width = width
                layoutParams.height = height
                requestLayout()
                false
            }
        }

        val conn = suspendCoroutine { continuation ->
            context.bindService(
                Intent().setClassName(pluginPackageName, AudioPluginViewService::class.java.name),
                HostConnection {
                    continuation.resume(it)
                },
                Context.BIND_AUTO_CREATE
            )
        }
        val message = Message.obtain().apply {
            data = bundleOf(
                AudioPluginViewService.MESSAGE_KEY_OPCODE to 0,
                AudioPluginViewService.MESSAGE_KEY_HOST_TOKEN to view.hostToken,
                AudioPluginViewService.MESSAGE_KEY_DISPLAY_ID to view.display.displayId,
                AudioPluginViewService.MESSAGE_KEY_PLUGIN_ID to pluginId,
                AudioPluginViewService.MESSAGE_KEY_INSTANCE_ID to instanceId,
                AudioPluginViewService.MESSAGE_KEY_WIDTH to view.width,
                AudioPluginViewService.MESSAGE_KEY_HEIGHT to view.height
            )
            replyTo = incomingMessenger
        }

        assert(message.replyTo != null)
        conn.outgoingMessenger.send(message)
    }

    class HostConnection(private val onConnected: (HostConnection) -> Unit) : ServiceConnection {
        lateinit var outgoingMessenger: Messenger

        override fun onServiceConnected(name: ComponentName?, service: IBinder?) {
            outgoingMessenger = Messenger(service)
            onConnected(this)
        }

        override fun onServiceDisconnected(name: ComponentName?) {
        }
    }

    class ClientReplyHandler(looper: Looper, private val onSurfacePackageReceived: (SurfaceControlViewHost.SurfacePackage) -> Unit) : Handler(looper) {
        override fun handleMessage(msg: Message) {
            val pkg = msg.data.getParcelable("surfacePackage") as SurfaceControlViewHost.SurfacePackage?
            pkg?.let { onSurfacePackageReceived(it) }
        }
    }
}