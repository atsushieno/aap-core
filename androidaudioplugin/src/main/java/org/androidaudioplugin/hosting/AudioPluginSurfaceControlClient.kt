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
import android.os.MessageQueue.IdleHandler
import android.os.Messenger
import android.util.Log
import android.view.SurfaceControlViewHost
import android.view.SurfaceView
import android.view.View
import android.view.ViewGroup.LayoutParams
import android.widget.LinearLayout
import androidx.annotation.RequiresApi
import androidx.annotation.WorkerThread
import androidx.core.os.bundleOf
import androidx.core.view.isEmpty
import kotlinx.coroutines.DelicateCoroutinesApi
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.GlobalScope
import kotlinx.coroutines.currentCoroutineContext
import kotlinx.coroutines.launch
import kotlinx.coroutines.runBlocking
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock
import org.androidaudioplugin.AudioPluginViewService
import kotlin.coroutines.resume
import kotlin.coroutines.suspendCoroutine

class AudioPluginSurfaceControlClient(private val context: Context) : AutoCloseable {
    companion object {
        val LOG_TAG = "AudioPluginSurfaceControlClient"

        val alwaysReconnectSurfaceControl = Build.VERSION.SDK_INT <= Build.VERSION_CODES.TIRAMISU
    }

    internal class AudioPluginSurfaceView(context: Context) : SurfaceView(context) {
        var connection: HostConnection? = null

        override fun onDetachedFromWindow() {
            super.onDetachedFromWindow()
            val conn = connection
            if (conn != null)
                context.unbindService(conn)
        }
        init {
            setZOrderOnTop(true)
            // FIXME: enable this when our compileSdk = 34 or later
            //if (Build.VERSION.SDK_INT > Build.VERSION_CODES.TIRAMISU)
            //    setSurfaceLifecycle(SURFACE_LIFECYCLE_FOLLOWS_ATTACHMENT)
        }
    }

    private val messageHandlerThread = HandlerThread("IncomingMessengerHandler").apply { start() }
    private val incomingMessenger = Messenger(ClientReplyHandler(messageHandlerThread.looper) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            surface.setChildSurfacePackage(it)
            println("client: surfaceView.setChildSurfacePackage() done.")
        }
    })

    private var surface = createSurfaceView()
    val surfaceView: View
        get() = surface

    private fun createSurfaceView() = AudioPluginSurfaceView(context)

    // It is intended to be invoked via AAPJniFacade, not for general users.
    @RequiresApi(Build.VERSION_CODES.R)
    fun connectUIAsync(pluginPackageName: String, pluginId: String, instanceId: Int, width: Int, height: Int) {
        // fire and forget!
        GlobalScope.launch {
            connectUI(pluginPackageName, pluginId, instanceId, width, height)
        }
    }

    @WorkerThread
    @RequiresApi(Build.VERSION_CODES.R)
    suspend fun connectUI(pluginPackageName: String, pluginId: String, instanceId: Int, width: Int, height: Int) {
        surface.apply {
            var handler: (() -> Boolean)? = null
            handler = {
                if (surface.layoutParams == null) { // resubmit it
                    Log.w(LOG_TAG, "It seems SurfaceView is created but not initialized yet. Resubmitting messaging handler")
                    context.mainLooper.queue.addIdleHandler(handler!!)
                } else {
                    surface.layoutParams.width = width
                    surface.layoutParams.height = height
                    requestLayout()
                }
                false
            }
            // This needs to be handled after layoutParams is initialized.
            context.mainLooper.queue.addIdleHandler (handler)
        }

        surface.connection = suspendCoroutine { continuation ->
             context.bindService(
                Intent().setClassName(pluginPackageName, AudioPluginViewService::class.java.name),
                HostConnection(onConnected = { continuation.resume(it) }),
                Context.BIND_AUTO_CREATE
            )
        }

        var messageSender: (() -> Boolean)? = null
        messageSender = {
            if (surface.display == null) { // resubmit it
                Log.w(LOG_TAG, "It seems SurfaceView is not attached to certain display. Resubmitting messaging handler")
                context.mainLooper.queue.addIdleHandler(messageSender!!)
            } else {
                val message = Message.obtain().apply {
                    data = bundleOf(
                        AudioPluginViewService.MESSAGE_KEY_OPCODE to 0,
                        AudioPluginViewService.MESSAGE_KEY_HOST_TOKEN to surface.hostToken,
                        AudioPluginViewService.MESSAGE_KEY_DISPLAY_ID to surface.display.displayId,
                        AudioPluginViewService.MESSAGE_KEY_PLUGIN_ID to pluginId,
                        AudioPluginViewService.MESSAGE_KEY_INSTANCE_ID to instanceId,
                        AudioPluginViewService.MESSAGE_KEY_WIDTH to surface.width,
                        AudioPluginViewService.MESSAGE_KEY_HEIGHT to surface.height
                    )
                    replyTo = incomingMessenger
                }

                surface.connection?.outgoingMessenger?.send(message)
            }
            false
        }
        context.mainLooper.queue.addIdleHandler (messageSender)
    }

    internal class HostConnection(private val onConnected: (HostConnection) -> Unit,
        private val onDisconnected: (HostConnection) -> Unit = {}) : ServiceConnection {
        lateinit var outgoingMessenger: Messenger

        override fun onServiceConnected(name: ComponentName?, service: IBinder?) {
            outgoingMessenger = Messenger(service)
            Log.d(LOG_TAG, "connected to ${AudioPluginViewService::class.java.name}")
            onConnected(this)
        }

        override fun onServiceDisconnected(name: ComponentName?) {
            onDisconnected(this)
            Log.d(LOG_TAG, "disconnected from ${AudioPluginViewService::class.java.name}")
        }
    }

    internal class ClientReplyHandler(looper: Looper, private val onSurfacePackageReceived: (SurfaceControlViewHost.SurfacePackage) -> Unit) : Handler(looper) {
        override fun handleMessage(msg: Message) {
            val pkg = msg.data.getParcelable("surfacePackage") as SurfaceControlViewHost.SurfacePackage?
            pkg?.let { onSurfacePackageReceived(it) }
        }
    }

    override fun close() {
        surface.connection?.let { context.unbindService(it) }
    }
}