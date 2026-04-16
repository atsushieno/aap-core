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
            if (conn != null) {
                connection = null
                context.unbindService(conn)
            }
        }
        init {
            setZOrderOnTop(true)
            // FIXME: enable this when our compileSdk = 34 or later
            //if (Build.VERSION.SDK_INT > Build.VERSION_CODES.TIRAMISU)
            //    setSurfaceLifecycle(SURFACE_LIFECYCLE_FOLLOWS_ATTACHMENT)
        }
    }

    private val messageHandlerThread = HandlerThread("IncomingMessengerHandler").apply { start() }
    private val incomingMessenger = Messenger(ClientReplyHandler(messageHandlerThread.looper) { guiInstanceId, surfacePackage ->
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            connectedGuiInstanceId = guiInstanceId
            this.surfacePackage = surfacePackage
            surface.setChildSurfacePackage(surfacePackage)
            pendingViewportConfiguration?.let(::sendConfigureViewport)
            Log.d(LOG_TAG, "client: surfaceView.setChildSurfacePackage() done.")
            connectedListeners.forEach { it() }
        }
    })

    val connectedListeners = mutableListOf<() -> Unit>()

    private var surface = createSurfaceView()
    private var surfacePackage: SurfaceControlViewHost.SurfacePackage? = null
    private var pendingViewportConfiguration: ViewportConfiguration? = null
    private var connectedPluginId: String? = null
    private var connectedInstanceId: Int = -1
    private var connectedGuiInstanceId: Int = -1
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
        var handler: (() -> Boolean)? = null
        handler = {
            if (surface.layoutParams == null) { // resubmit it
                Log.w(LOG_TAG, "It seems SurfaceView is created but not initialized yet. It is most likely that it is not attached to a live view tree. Resubmitting messaging handler")
                context.mainLooper.queue.addIdleHandler(handler!!)
            } else {
                connectUIPrepareLayout(width, height)
            }
            false
        }
        // This needs to be handled after layoutParams is initialized.
        context.mainLooper.queue.addIdleHandler(handler)

        connectUIBindService(pluginPackageName)

        var messageSender: (() -> Boolean)? = null
        messageSender = {
            if (surface.display == null) { // resubmit it
                Log.w(LOG_TAG, "SurfaceView is not attached to a display. It is most likely that it is not attached to a live view tree. Resubmitting messaging handler")
                context.mainLooper.queue.addIdleHandler(messageSender!!)
            } else {
                connectUISendMessage(pluginId, instanceId)
            }
            false
        }
        context.mainLooper.queue.addIdleHandler (messageSender)
    }

    // connectUI without idle handler
    @WorkerThread
    @RequiresApi(Build.VERSION_CODES.R)
    suspend fun connectUINoHandler(pluginPackageName: String, pluginId: String, instanceId: Int, width: Int, height: Int) {
        connectUIPrepareLayout(width, height)

        connectUIBindService(pluginPackageName)

        connectUISendMessage(pluginId, instanceId)
    }

        private fun connectUIPrepareLayout(width: Int, height: Int) {
            surface.layoutParams.width = width
            surface.layoutParams.height = height
            surface.requestLayout()
        }

        @RequiresApi(Build.VERSION_CODES.R)
        private suspend fun connectUIBindService(pluginPackageName: String) {
            surface.connection = suspendCoroutine { continuation ->
                context.bindService(
                    Intent().setClassName(pluginPackageName, AudioPluginViewService::class.java.name),
                    HostConnection(onConnected = { continuation.resume(it) }),
                    Context.BIND_AUTO_CREATE
                )
            }
        }

        @RequiresApi(Build.VERSION_CODES.R)
        private fun connectUISendMessage(pluginId: String, instanceId: Int) {
            connectedPluginId = pluginId
            connectedInstanceId = instanceId
            val message = Message.obtain().apply {
                data = bundleOf(
                    AudioPluginViewService.MESSAGE_KEY_OPCODE to AudioPluginViewService.OPCODE_CONNECT,
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

    @RequiresApi(Build.VERSION_CODES.R)
    fun resizeUI(instanceId: Int, width: Int, height: Int) {
        connectUIPrepareLayout(width, height)
        resizeRemoteUI(instanceId, width, height)
    }

    @RequiresApi(Build.VERSION_CODES.R)
    fun configureViewport(instanceId: Int, viewportWidth: Int, viewportHeight: Int,
                          contentWidth: Int, contentHeight: Int, scrollX: Int, scrollY: Int) {
        connectUIPrepareLayout(viewportWidth, viewportHeight)
        val configuration = ViewportConfiguration(
            instanceId,
            viewportWidth,
            viewportHeight,
            contentWidth,
            contentHeight,
            scrollX,
            scrollY
        )
        pendingViewportConfiguration = configuration
        sendConfigureViewport(configuration)
    }

    @RequiresApi(Build.VERSION_CODES.R)
    private fun resizeRemoteUI(instanceId: Int, width: Int, height: Int) {
        val message = Message.obtain().apply {
            data = bundleOf(
                AudioPluginViewService.MESSAGE_KEY_OPCODE to AudioPluginViewService.OPCODE_RESIZE,
                AudioPluginViewService.MESSAGE_KEY_INSTANCE_ID to instanceId,
                AudioPluginViewService.MESSAGE_KEY_WIDTH to width,
                AudioPluginViewService.MESSAGE_KEY_HEIGHT to height
            )
        }
        surface.connection?.outgoingMessenger?.send(message)
    }

    @RequiresApi(Build.VERSION_CODES.R)
    private fun sendConfigureViewport(configuration: ViewportConfiguration) {
        val outgoingMessenger = surface.connection?.outgoingMessenger ?: return
        val message = Message.obtain().apply {
            data = bundleOf(
                AudioPluginViewService.MESSAGE_KEY_OPCODE to AudioPluginViewService.OPCODE_CONFIGURE_VIEWPORT,
                AudioPluginViewService.MESSAGE_KEY_INSTANCE_ID to configuration.instanceId,
                AudioPluginViewService.MESSAGE_KEY_VIEWPORT_WIDTH to configuration.viewportWidth,
                AudioPluginViewService.MESSAGE_KEY_VIEWPORT_HEIGHT to configuration.viewportHeight,
                AudioPluginViewService.MESSAGE_KEY_CONTENT_WIDTH to configuration.contentWidth,
                AudioPluginViewService.MESSAGE_KEY_CONTENT_HEIGHT to configuration.contentHeight,
                AudioPluginViewService.MESSAGE_KEY_SCROLL_X to configuration.scrollX,
                AudioPluginViewService.MESSAGE_KEY_SCROLL_Y to configuration.scrollY
            )
        }
        outgoingMessenger.send(message)
    }

    @RequiresApi(Build.VERSION_CODES.R)
    private fun disconnectRemoteUI() {
        val outgoingMessenger = surface.connection?.outgoingMessenger ?: return
        val pluginId = connectedPluginId ?: return
        if (connectedInstanceId < 0)
            return

        val message = Message.obtain().apply {
            data = bundleOf(
                AudioPluginViewService.MESSAGE_KEY_OPCODE to AudioPluginViewService.OPCODE_DISCONNECT,
                AudioPluginViewService.MESSAGE_KEY_PLUGIN_ID to pluginId,
                AudioPluginViewService.MESSAGE_KEY_INSTANCE_ID to connectedInstanceId,
                AudioPluginViewService.MESSAGE_KEY_GUI_INSTANCE_ID to connectedGuiInstanceId
            )
        }
        outgoingMessenger.send(message)
    }

    private data class ViewportConfiguration(
        val instanceId: Int,
        val viewportWidth: Int,
        val viewportHeight: Int,
        val contentWidth: Int,
        val contentHeight: Int,
        val scrollX: Int,
        val scrollY: Int
    )

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

    internal class ClientReplyHandler(looper: Looper, private val onSurfacePackageReceived: (Int, SurfaceControlViewHost.SurfacePackage) -> Unit) : Handler(looper) {
        override fun handleMessage(msg: Message) {
            val guiInstanceId = msg.data.getInt(AudioPluginViewService.MESSAGE_KEY_GUI_INSTANCE_ID)
            val pkg = msg.data.getParcelable(AudioPluginViewService.MESSAGE_KEY_SURFACE_PACKAGE) as SurfaceControlViewHost.SurfacePackage?
            pkg?.let { onSurfacePackageReceived(guiInstanceId, it) }
        }
    }

    override fun close() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R)
            disconnectRemoteUI()
        surfacePackage?.release()
        surfacePackage = null
        pendingViewportConfiguration = null
        connectedPluginId = null
        connectedInstanceId = -1
        connectedGuiInstanceId = -1
        surface.connection?.let {
            surface.connection = null
            context.unbindService(it)
        }
        messageHandlerThread.quitSafely()
    }
}
