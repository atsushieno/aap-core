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
import android.os.RemoteException
import android.util.Log
import android.view.KeyEvent
import android.view.MotionEvent
import android.view.SurfaceControlViewHost
import android.view.SurfaceView
import android.view.View
import android.view.ViewGroup.LayoutParams
import android.view.WindowInsets
import android.view.WindowManager
import android.window.OnBackInvokedCallback
import android.window.OnBackInvokedDispatcher
import android.widget.LinearLayout
import androidx.annotation.RequiresApi
import androidx.annotation.WorkerThread
import androidx.core.os.bundleOf
import androidx.core.view.isEmpty
import kotlinx.coroutines.DelicateCoroutinesApi
import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.GlobalScope
import kotlinx.coroutines.currentCoroutineContext
import kotlinx.coroutines.launch
import kotlinx.coroutines.runBlocking
import kotlinx.coroutines.withTimeoutOrNull
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock
import org.androidaudioplugin.AudioPluginViewService
import kotlin.coroutines.resume
import kotlin.coroutines.suspendCoroutine

class AudioPluginSurfaceControlClient(private val context: Context) : AutoCloseable {
    companion object {
        val LOG_TAG = "AudioPluginSurfaceControlClient"

        val alwaysReconnectSurfaceControl = Build.VERSION.SDK_INT <= Build.VERSION_CODES.TIRAMISU

        private const val FOCUS_REQUEST_RETRY_MILLIS = 100L
        private const val FOCUS_REQUEST_MAX_ATTEMPTS = 20

        private val viewToClient = java.util.Collections.synchronizedMap(java.util.WeakHashMap<android.view.View, AudioPluginSurfaceControlClient>())

        /** Returns the [AudioPluginSurfaceControlClient] that owns [view], or null if not known. */
        @JvmStatic
        fun fromSurfaceView(view: android.view.View): AudioPluginSurfaceControlClient? = viewToClient[view]
    }

    internal class AudioPluginSurfaceView(context: Context, private val owner: AudioPluginSurfaceControlClient) : SurfaceView(context) {
        var connection: HostConnection? = null

        // Back arriving at this window while the remote editor's IME is up was forwarded here by
        // the platform: the embedded window's ViewRootImpl intercepts KEYCODE_BACK in its pre-IME
        // stage and hands it to the host (SurfaceView.forwardBackKeyToParent), so neither the
        // plugin's view tree nor the IME ever gets the chance to consume it. Left alone it reaches
        // the host's own back handling, where "dismiss the keyboard" can mean "finish the Activity".
        // Intercept it while, and only while, the IME is actually showing.
        private var backCallback: OnBackInvokedCallback? = null

        private fun setImeBackInterceptionEnabled(enabled: Boolean) {
            if (Build.VERSION.SDK_INT < Build.VERSION_CODES.TIRAMISU)
                return
            val dispatcher = findOnBackInvokedDispatcher() ?: return
            if (enabled) {
                if (backCallback != null)
                    return
                val callback = OnBackInvokedCallback { owner.requestHideRemoteIme() }
                dispatcher.registerOnBackInvokedCallback(
                    OnBackInvokedDispatcher.PRIORITY_OVERLAY, callback)
                backCallback = callback
            } else {
                backCallback?.let { dispatcher.unregisterOnBackInvokedCallback(it) }
                backCallback = null
            }
        }

        override fun onApplyWindowInsets(insets: WindowInsets): WindowInsets {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
                val imeVisible = insets.isVisible(WindowInsets.Type.ime())
                Log.d(LOG_TAG, "Native UI host insets: imeVisible=$imeVisible")
                setImeBackInterceptionEnabled(imeVisible && owner.surfacePackage != null)
            }
            return super.onApplyWindowInsets(insets)
        }

        // Hosts below API 33 have no OnBackInvokedDispatcher, so back still arrives through key
        // dispatch there. Native UI itself requires API 30, hence the lower bound.
        override fun onKeyPreIme(keyCode: Int, event: KeyEvent): Boolean {
            if (keyCode == KeyEvent.KEYCODE_BACK &&
                Build.VERSION.SDK_INT >= Build.VERSION_CODES.R &&
                Build.VERSION.SDK_INT < Build.VERSION_CODES.TIRAMISU &&
                owner.surfacePackage != null &&
                isRemoteImeVisible()) {
                if (event.action == KeyEvent.ACTION_UP)
                    owner.requestHideRemoteIme()
                return true
            }
            return super.onKeyPreIme(keyCode, event)
        }

        @RequiresApi(Build.VERSION_CODES.R)
        private fun isRemoteImeVisible() =
            rootWindowInsets?.isVisible(WindowInsets.Type.ime()) == true

        override fun onDetachedFromWindow() {
            setImeBackInterceptionEnabled(false)
            super.onDetachedFromWindow()
            owner.handleSurfaceDetachedFromWindow()
        }

        override fun onTouchEvent(event: MotionEvent): Boolean {
            if (event.action == MotionEvent.ACTION_DOWN &&
                Build.VERSION.SDK_INT >= Build.VERSION_CODES.VANILLA_ICE_CREAM) {
                val from = rootSurfaceControl?.inputTransferToken
                val to = owner.surfacePackage?.inputTransferToken
                if (from != null && to != null) {
                    val wm = context.getSystemService(WindowManager::class.java)
                    val ok = wm?.transferTouchGesture(from, to) ?: false
                    if (!ok)
                        Log.w(LOG_TAG, "transferTouchGesture failed; embedded view may not receive input as expected")
                }
            }
            return super.onTouchEvent(event)
        }

        init {
            isFocusable = true
            isFocusableInTouchMode = true
            // View.onTouchEvent() only takes focus from within its `clickable` branch, so a
            // plain SurfaceView never becomes the focused view on tap and thus never triggers
            // SurfaceView.onFocusChanged() -> grantEmbeddedWindowFocus(). Without that the
            // embedded window is not an IME target and remote text fields cannot show the
            // software keyboard.
            isClickable = true
            setZOrderOnTop(true)
            // FIXME: enable this when our compileSdk = 34 or later
            //if (Build.VERSION.SDK_INT > Build.VERSION_CODES.TIRAMISU)
            //    setSurfaceLifecycle(SURFACE_LIFECYCLE_FOLLOWS_ATTACHMENT)
        }
    }

    private val messageHandlerThread = HandlerThread("IncomingMessengerHandler").apply { start() }
    private val incomingMessenger = Messenger(ClientReplyHandler(
        messageHandlerThread.looper,
        onSurfacePackageReceived = onSurfacePackageReceived@ { guiSessionId, pluginId, instanceId, surfacePackage ->
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
                if (!isExpectedReply(pluginId, instanceId, null)) {
                    Log.w(LOG_TAG, "Ignoring surface package for unexpected UI route pluginId:$pluginId instanceId:$instanceId guiSessionId:$guiSessionId expectedPluginId:$connectedPluginId expectedInstanceId:$connectedInstanceId")
                    return@onSurfacePackageReceived
                }
                connectedGuiSessionId = guiSessionId
                this.surfacePackage?.release()
                this.surfacePackage = surfacePackage
                getOrCreateSurfaceView().setChildSurfacePackage(surfacePackage)
                requestEmbeddedUIFocus()
                pendingViewportConfiguration?.let(::sendConfigureViewport)
                Log.i(LOG_TAG, "accepted surface package pluginId:$pluginId instanceId:$instanceId guiSessionId:$guiSessionId")
                connectedListeners.forEach { it() }
            }
        },
        onContentSizeChanged = onContentSizeChanged@ { guiSessionId, pluginId, instanceId, width, height ->
            if (!isExpectedReply(pluginId, instanceId, guiSessionId)) {
                Log.w(LOG_TAG, "Ignoring content size for unexpected UI route pluginId:$pluginId instanceId:$instanceId guiSessionId:$guiSessionId expectedPluginId:$connectedPluginId expectedInstanceId:$connectedInstanceId expectedGuiSessionId:$connectedGuiSessionId")
                return@onContentSizeChanged
            }
            contentSizeChangedListeners.forEach { it(width, height) }
        },
        onFocusRequested = {
            requestEmbeddedUIFocus()
        }
    ))

    val connectedListeners = mutableListOf<() -> Unit>()
    val contentSizeChangedListeners = mutableListOf<(Int, Int) -> Unit>()

    private var surface: AudioPluginSurfaceView? = null
    private var surfacePackage: SurfaceControlViewHost.SurfacePackage? = null
    private var pendingViewportConfiguration: ViewportConfiguration? = null
    private var connectedPluginId: String? = null
    private var connectedInstanceId: Int = -1
    private var connectedGuiSessionId: Int = -1
    val surfaceView: View
        get() = getOrCreateSurfaceView()

    private fun createSurfaceView() = AudioPluginSurfaceView(context, this).also { viewToClient[it] = this }

    private fun getOrCreateSurfaceView(): AudioPluginSurfaceView =
        surface ?: createSurfaceView().also { surface = it }

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
            val surfaceView = getOrCreateSurfaceView()
            if (surfaceView.layoutParams == null) { // resubmit it
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
            if (getOrCreateSurfaceView().display == null) { // resubmit it
                Log.w(LOG_TAG, "SurfaceView is not attached to a display. It is most likely that it is not attached to a live view tree. Resubmitting messaging handler")
                context.mainLooper.queue.addIdleHandler(messageSender!!)
            } else {
                connectUISendMessage(pluginId, instanceId, width, height)
            }
            false
        }
        context.mainLooper.queue.addIdleHandler (messageSender)
    }

    @RequiresApi(Build.VERSION_CODES.R)
    fun getPreferredSize(pluginPackageName: String, pluginId: String, instanceId: Int): IntArray =
        runBlocking {
            getPreferredSizeNoHandler(pluginPackageName, pluginId, instanceId)
        }

    @WorkerThread
    @RequiresApi(Build.VERSION_CODES.R)
    suspend fun getPreferredSizeNoHandler(pluginPackageName: String, pluginId: String, instanceId: Int): IntArray {
        val connection = bindPluginViewService(pluginPackageName)
        val result = CompletableDeferred<IntArray>()
        val replyMessenger = Messenger(object : Handler(messageHandlerThread.looper) {
            override fun handleMessage(msg: Message) {
                result.complete(intArrayOf(
                    msg.data.getInt(AudioPluginViewService.MESSAGE_KEY_PREFERRED_WIDTH),
                    msg.data.getInt(AudioPluginViewService.MESSAGE_KEY_PREFERRED_HEIGHT)
                ))
            }
        })
        try {
            connection.outgoingMessenger.send(Message.obtain().apply {
                data = bundleOf(
                    AudioPluginViewService.MESSAGE_KEY_OPCODE to AudioPluginViewService.OPCODE_GET_PREFERRED_SIZE,
                    AudioPluginViewService.MESSAGE_KEY_PLUGIN_ID to pluginId,
                    AudioPluginViewService.MESSAGE_KEY_INSTANCE_ID to instanceId
                )
                replyTo = replyMessenger
            })
            return withTimeoutOrNull(3000) { result.await() } ?: intArrayOf(0, 0)
        } finally {
            connection.unbind(context)
        }
    }

    // connectUI without idle handler
    @WorkerThread
    @RequiresApi(Build.VERSION_CODES.R)
    suspend fun connectUINoHandler(pluginPackageName: String, pluginId: String, instanceId: Int, width: Int, height: Int) {
        connectUIPrepareLayout(width, height)

        connectUIBindService(pluginPackageName)

        connectUISendMessage(pluginId, instanceId, width, height)
    }

        private fun connectUIPrepareLayout(width: Int, height: Int) {
            val surfaceView = getOrCreateSurfaceView()
            val layoutParams = surfaceView.layoutParams
                ?: LayoutParams(width, height).also { surfaceView.layoutParams = it }
            layoutParams.width = width
            layoutParams.height = height
            surfaceView.requestLayout()
        }

        @RequiresApi(Build.VERSION_CODES.R)
        private suspend fun connectUIBindService(pluginPackageName: String) {
            getOrCreateSurfaceView().connection = bindPluginViewService(pluginPackageName)
        }

        @RequiresApi(Build.VERSION_CODES.R)
        private suspend fun bindPluginViewService(pluginPackageName: String): HostConnection =
            suspendCoroutine { continuation ->
                val connection = HostConnection(
                    onConnected = { continuation.resume(it) },
                    onDisconnected = { handleRemoteUIDisconnected(it, "service disconnected") }
                )
                if (!context.bindService(
                        Intent().setClassName(pluginPackageName, AudioPluginViewService::class.java.name),
                        connection,
                        Context.BIND_AUTO_CREATE
                    )
                ) {
                    throw IllegalStateException("Failed to bind AudioPluginViewService for $pluginPackageName")
                }
            }

        @RequiresApi(Build.VERSION_CODES.R)
        private fun connectUISendMessage(pluginId: String, instanceId: Int, width: Int, height: Int) {
            val surfaceView = getOrCreateSurfaceView()
            connectedPluginId = pluginId
            connectedInstanceId = instanceId
            val data = bundleOf(
                AudioPluginViewService.MESSAGE_KEY_OPCODE to AudioPluginViewService.OPCODE_CONNECT,
                AudioPluginViewService.MESSAGE_KEY_HOST_TOKEN to surfaceView.hostToken,
                AudioPluginViewService.MESSAGE_KEY_DISPLAY_ID to surfaceView.display.displayId,
                AudioPluginViewService.MESSAGE_KEY_PLUGIN_ID to pluginId,
                AudioPluginViewService.MESSAGE_KEY_INSTANCE_ID to instanceId,
                AudioPluginViewService.MESSAGE_KEY_WIDTH to width,
                AudioPluginViewService.MESSAGE_KEY_HEIGHT to height
            )
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.VANILLA_ICE_CREAM) {
                surfaceView.rootSurfaceControl?.inputTransferToken?.let {
                    data.putParcelable(AudioPluginViewService.MESSAGE_KEY_INPUT_TRANSFER_TOKEN, it)
                }
            }
            val message = Message.obtain().apply {
                this.data = data
                replyTo = incomingMessenger
            }
            sendToCurrentConnection(message, "connect")
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
                AudioPluginViewService.MESSAGE_KEY_GUI_SESSION_ID to connectedGuiSessionId,
                AudioPluginViewService.MESSAGE_KEY_WIDTH to width,
                AudioPluginViewService.MESSAGE_KEY_HEIGHT to height
            )
        }
        sendToCurrentConnection(message, "resize")
    }

    @RequiresApi(Build.VERSION_CODES.R)
    private fun sendConfigureViewport(configuration: ViewportConfiguration) {
        val outgoingMessenger = surface?.connection?.outgoingMessenger ?: return
        val message = Message.obtain().apply {
            data = bundleOf(
                AudioPluginViewService.MESSAGE_KEY_OPCODE to AudioPluginViewService.OPCODE_CONFIGURE_VIEWPORT,
                AudioPluginViewService.MESSAGE_KEY_INSTANCE_ID to configuration.instanceId,
                AudioPluginViewService.MESSAGE_KEY_GUI_SESSION_ID to connectedGuiSessionId,
                AudioPluginViewService.MESSAGE_KEY_VIEWPORT_WIDTH to configuration.viewportWidth,
                AudioPluginViewService.MESSAGE_KEY_VIEWPORT_HEIGHT to configuration.viewportHeight,
                AudioPluginViewService.MESSAGE_KEY_CONTENT_WIDTH to configuration.contentWidth,
                AudioPluginViewService.MESSAGE_KEY_CONTENT_HEIGHT to configuration.contentHeight,
                AudioPluginViewService.MESSAGE_KEY_SCROLL_X to configuration.scrollX,
                AudioPluginViewService.MESSAGE_KEY_SCROLL_Y to configuration.scrollY
            )
        }
        sendToCurrentConnection(message, "configureViewport")
    }

    @RequiresApi(Build.VERSION_CODES.R)
    private fun disconnectRemoteUI() {
        val surfaceView = surface ?: return
        val outgoingMessenger = surfaceView.connection?.outgoingMessenger ?: return
        val pluginId = connectedPluginId ?: return
        if (connectedInstanceId < 0)
            return

        val message = Message.obtain().apply {
            data = bundleOf(
                AudioPluginViewService.MESSAGE_KEY_OPCODE to AudioPluginViewService.OPCODE_DISCONNECT,
                AudioPluginViewService.MESSAGE_KEY_PLUGIN_ID to pluginId,
                AudioPluginViewService.MESSAGE_KEY_INSTANCE_ID to connectedInstanceId,
                AudioPluginViewService.MESSAGE_KEY_GUI_SESSION_ID to connectedGuiSessionId
            )
        }
        try {
            outgoingMessenger.send(message)
        } catch (ex: RemoteException) {
            Log.w(LOG_TAG, "disconnectRemoteUI ignored after remote UI process loss", ex)
            handleRemoteUIDisconnected(surfaceView.connection, "disconnect send failed")
        }
    }

    // SurfaceView.setChildSurfacePackage() asks WindowManager to make the embedded
    // SurfaceControlViewHost window the focus (and therefore IME) target, but only when the
    // SurfaceView already holds view focus; later changes go through onFocusChanged(). Nothing
    // else requests that focus here: the embedded surface is Z-ordered on top and consumes the
    // gesture, so the host SurfaceView may never see a touch at all. Request it explicitly.
    //
    // The surface package can arrive before the host has the SurfaceView attached, visible and
    // laid out (hosts that build the container first and reveal it afterwards do exactly that),
    // and requestFocus() is a no-op until then. So retry while the view is not ready yet, but
    // only attempt the request itself once: if the host refuses focus when the view *is* ready,
    // that is the host's decision and we must not fight it.
    private fun requestEmbeddedUIFocus() {
        val surfaceView = surface ?: return
        val handler = Handler(context.mainLooper)
        var remainingAttempts = FOCUS_REQUEST_MAX_ATTEMPTS
        lateinit var attempt: Runnable
        attempt = Runnable {
            // give up if this client moved on to another SurfaceView or was closed
            if (surface !== surfaceView || surfacePackage == null)
                return@Runnable
            if (surfaceView.isFocused)
                return@Runnable
            val ready = surfaceView.isAttachedToWindow &&
                    surfaceView.visibility == View.VISIBLE &&
                    surfaceView.width > 0 && surfaceView.height > 0
            if (!ready) {
                if (--remainingAttempts > 0)
                    handler.postDelayed(attempt, FOCUS_REQUEST_RETRY_MILLIS)
                else
                    Log.w(LOG_TAG, "SurfaceView never became ready to take focus; remote UI will not be an IME target until it is touched")
                return@Runnable
            }
            if (!surfaceView.requestFocus())
                Log.i(LOG_TAG, "Host declined focus for the Native UI SurfaceView; remote text input stays inactive until it is focused")
        }
        handler.post(attempt)
    }

    @RequiresApi(Build.VERSION_CODES.R)
    internal fun requestHideRemoteIme() {
        val pluginId = connectedPluginId ?: return
        val message = Message.obtain().apply {
            data = bundleOf(
                AudioPluginViewService.MESSAGE_KEY_OPCODE to AudioPluginViewService.OPCODE_HIDE_IME,
                AudioPluginViewService.MESSAGE_KEY_PLUGIN_ID to pluginId,
                AudioPluginViewService.MESSAGE_KEY_INSTANCE_ID to connectedInstanceId,
                AudioPluginViewService.MESSAGE_KEY_GUI_SESSION_ID to connectedGuiSessionId
            )
        }
        sendToCurrentConnection(message, "hideIme")
    }

    private fun handleSurfaceDetachedFromWindow() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            disconnectRemoteUI()
            surfacePackage?.release()
        }
        surfacePackage = null
        pendingViewportConfiguration = null
        connectedPluginId = null
        connectedInstanceId = -1
        connectedGuiSessionId = -1
        surface?.connection?.let {
            surface?.connection = null
            it.unbind(context)
        }
    }

    private fun sendToCurrentConnection(message: Message, operation: String) {
        val connection = surface?.connection ?: return
        try {
            connection.outgoingMessenger.send(message)
        } catch (ex: RemoteException) {
            Log.w(LOG_TAG, "AudioPluginViewService $operation failed; invalidating dead remote UI connection", ex)
            handleRemoteUIDisconnected(connection, "$operation failed")
        }
    }

    private fun handleRemoteUIDisconnected(connection: HostConnection?, reason: String) {
        Log.w(LOG_TAG, "Remote plugin UI disconnected: $reason")
        val surfaceView = surface
        if (surfaceView != null && surfaceView.connection === connection)
            surfaceView.connection = null
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R)
            surfacePackage?.release()
        surfacePackage = null
        pendingViewportConfiguration = null
        connectedPluginId = null
        connectedInstanceId = -1
        connectedGuiSessionId = -1
        connection?.unbind(context)
    }

    private fun isExpectedReply(pluginId: String?, instanceId: Int?, guiSessionId: Int?): Boolean {
        val expectedPluginId = connectedPluginId
        if (pluginId != null && expectedPluginId != null && pluginId != expectedPluginId)
            return false
        if (instanceId != null && connectedInstanceId >= 0 && instanceId != connectedInstanceId)
            return false
        if (guiSessionId != null && connectedGuiSessionId >= 0 && guiSessionId != connectedGuiSessionId)
            return false
        return true
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
        var packageName: String? = null
            private set
        @Volatile
        private var isBound = true

        override fun onServiceConnected(name: ComponentName?, service: IBinder?) {
            outgoingMessenger = Messenger(service)
            packageName = name?.packageName
            Log.d(LOG_TAG, "connected to ${AudioPluginViewService::class.java.name}")
            onConnected(this)
        }

        override fun onServiceDisconnected(name: ComponentName?) {
            isBound = false
            onDisconnected(this)
            Log.d(LOG_TAG, "disconnected from ${AudioPluginViewService::class.java.name}")
        }

        override fun onBindingDied(name: ComponentName?) {
            isBound = false
            onDisconnected(this)
            Log.w(LOG_TAG, "binding died for ${AudioPluginViewService::class.java.name}")
        }

        override fun onNullBinding(name: ComponentName?) {
            isBound = false
            onDisconnected(this)
            Log.w(LOG_TAG, "null binding for ${AudioPluginViewService::class.java.name}")
        }

        fun unbind(context: Context) {
            if (!isBound)
                return
            synchronized(this) {
                if (!isBound)
                    return
                try {
                    context.unbindService(this)
                } catch (ex: IllegalArgumentException) {
                    Log.w(LOG_TAG, "Ignoring duplicate service unbind", ex)
                } finally {
                    isBound = false
                }
            }
        }
    }

    internal class ClientReplyHandler(
        looper: Looper,
        private val onSurfacePackageReceived: (Int, String?, Int?, SurfaceControlViewHost.SurfacePackage) -> Unit,
        private val onContentSizeChanged: (Int?, String?, Int?, Int, Int) -> Unit = { _, _, _, _, _ -> },
        private val onFocusRequested: () -> Unit = {}
    ) : Handler(looper) {
        override fun handleMessage(msg: Message) {
            when (msg.data.getInt(AudioPluginViewService.MESSAGE_KEY_OPCODE)) {
                AudioPluginViewService.OPCODE_REQUEST_FOCUS -> {
                    onFocusRequested()
                }
                AudioPluginViewService.OPCODE_CONTENT_SIZE_CHANGED -> {
                    val width = msg.data.getInt(AudioPluginViewService.MESSAGE_KEY_CONTENT_WIDTH)
                    val height = msg.data.getInt(AudioPluginViewService.MESSAGE_KEY_CONTENT_HEIGHT)
                    val guiSessionId = readGuiSessionId(msg)
                    val pluginId = msg.data.getString(AudioPluginViewService.MESSAGE_KEY_PLUGIN_ID)
                    val instanceId = if (msg.data.containsKey(AudioPluginViewService.MESSAGE_KEY_INSTANCE_ID))
                        msg.data.getInt(AudioPluginViewService.MESSAGE_KEY_INSTANCE_ID)
                    else
                        null
                    onContentSizeChanged(guiSessionId, pluginId, instanceId, width, height)
                }
                else -> {
                    // OPCODE_CONNECT reply (and legacy replies without an explicit opcode, which default to 0)
                    val guiSessionId = readGuiSessionId(msg) ?: -1
                    val pluginId = msg.data.getString(AudioPluginViewService.MESSAGE_KEY_PLUGIN_ID)
                    val instanceId = if (msg.data.containsKey(AudioPluginViewService.MESSAGE_KEY_INSTANCE_ID))
                        msg.data.getInt(AudioPluginViewService.MESSAGE_KEY_INSTANCE_ID)
                    else
                        null
                    val pkg = msg.data.getParcelable(AudioPluginViewService.MESSAGE_KEY_SURFACE_PACKAGE) as SurfaceControlViewHost.SurfacePackage?
                    pkg?.let { onSurfacePackageReceived(guiSessionId, pluginId, instanceId, it) }
                }
            }
        }

        private fun readGuiSessionId(msg: Message): Int? =
            if (msg.data.containsKey(AudioPluginViewService.MESSAGE_KEY_GUI_SESSION_ID))
                msg.data.getInt(AudioPluginViewService.MESSAGE_KEY_GUI_SESSION_ID)
            else if (msg.data.containsKey(AudioPluginViewService.LEGACY_MESSAGE_KEY_GUI_SESSION_ID))
                msg.data.getInt(AudioPluginViewService.LEGACY_MESSAGE_KEY_GUI_SESSION_ID)
            else
                null
    }

    override fun close() {
        handleSurfaceDetachedFromWindow()
        messageHandlerThread.quitSafely()
    }
}
