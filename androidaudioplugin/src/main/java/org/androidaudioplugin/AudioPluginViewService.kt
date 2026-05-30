package org.androidaudioplugin

import android.content.Intent
import android.hardware.display.DisplayManager
import android.os.Build
import android.os.Handler
import android.os.IBinder
import android.os.Looper
import android.os.Message
import android.os.Messenger
import android.os.RemoteException
import android.util.Log
import android.view.SurfaceControlViewHost
import android.view.View
import android.view.ViewGroup
import android.widget.FrameLayout
import android.window.InputTransferToken
import androidx.annotation.RequiresApi
import androidx.core.os.bundleOf
import androidx.lifecycle.LifecycleService
import androidx.lifecycle.setViewTreeLifecycleOwner
import androidx.savedstate.SavedStateRegistry
import androidx.savedstate.SavedStateRegistryController
import androidx.savedstate.SavedStateRegistryOwner
import androidx.savedstate.setViewTreeSavedStateRegistryOwner
import java.lang.IllegalArgumentException

/*
    One Service connection handles multiple UI instances, per instanceId.
    One AudioPluginGuiController is created for each plugin instance, and
    (so far) only one View host is instantiated.
 */
class AudioPluginViewService : LifecycleService(), SavedStateRegistryOwner {
    companion object {
        const val LOG_TAG = "AudioPluginViewService"

        const val OPCODE_CONNECT = 0
        const val OPCODE_DISCONNECT = 1
        const val OPCODE_RESIZE = 2
        const val OPCODE_CONFIGURE_VIEWPORT = 3
        const val OPCODE_GET_PREFERRED_SIZE = 4
        // reply sent from service to host when plugin view resizes itself
        const val OPCODE_CONTENT_SIZE_CHANGED = 5

        // requests
        const val MESSAGE_KEY_OPCODE = "opcode"
        const val MESSAGE_KEY_HOST_TOKEN = "hostToken"
        const val MESSAGE_KEY_INPUT_TRANSFER_TOKEN = "inputTransferToken"
        const val MESSAGE_KEY_DISPLAY_ID = "displayId"
        const val MESSAGE_KEY_PLUGIN_ID = "pluginId"
        const val MESSAGE_KEY_INSTANCE_ID = "instanceId"
        const val MESSAGE_KEY_WIDTH = "width"
        const val MESSAGE_KEY_HEIGHT = "height"
        const val MESSAGE_KEY_VIEWPORT_WIDTH = "viewportWidth"
        const val MESSAGE_KEY_VIEWPORT_HEIGHT = "viewportHeight"
        const val MESSAGE_KEY_CONTENT_WIDTH = "contentWidth"
        const val MESSAGE_KEY_CONTENT_HEIGHT = "contentHeight"
        const val MESSAGE_KEY_SCROLL_X = "scrollX"
        const val MESSAGE_KEY_SCROLL_Y = "scrollY"
        // replies
        const val MESSAGE_KEY_GUI_SESSION_ID = "guiSessionId"
        // Compatibility alias for older clients that still use the old wire name.
        const val LEGACY_MESSAGE_KEY_GUI_SESSION_ID = "guiInstanceId"
        const val MESSAGE_KEY_SURFACE_PACKAGE = "surfacePackage"
        const val MESSAGE_KEY_PREFERRED_WIDTH = "preferredWidth"
        const val MESSAGE_KEY_PREFERRED_HEIGHT = "preferredHeight"
    }

    private lateinit var messenger: Messenger
    lateinit var host: SurfaceControlViewHost

    private val savedStateRegistryController = SavedStateRegistryController.create(this)
    override val savedStateRegistry: SavedStateRegistry
        get() = savedStateRegistryController.savedStateRegistry

    override fun onCreate() {
        super.onCreate()

        savedStateRegistryController.performRestore(null)
        messenger = Messenger(PluginViewControllerHandler(Looper.myLooper()!!, this))
    }

    override fun onBind(intent: Intent): IBinder? {
        super.onBind(intent)
        return messenger.binder
    }

    private class PluginViewControllerHandler(looper: Looper, private val owner: AudioPluginViewService)
        : Handler(looper) {

        @RequiresApi(Build.VERSION_CODES.R)
        override fun handleMessage(msg: Message) {
            when (msg.data.getInt(MESSAGE_KEY_OPCODE)) {
                OPCODE_CONNECT -> {
                    owner.handleConnectRequest(msg)
                }
                OPCODE_DISCONNECT -> {
                    owner.handleDisconnectRequest(msg)
                }
                OPCODE_RESIZE -> {
                    owner.handleResizeRequest(msg)
                }
                OPCODE_CONFIGURE_VIEWPORT -> {
                    owner.handleConfigureViewportRequest(msg)
                }
                OPCODE_GET_PREFERRED_SIZE -> {
                    owner.handleGetPreferredSizeRequest(msg)
                }
                else -> {}
            }
        }
    }

    private val controllers = mutableMapOf<Int,Controller>()
    private var nextGuiSessionId = 1
    private var nextGuiInstanceId = 1

    private fun handleConnectRequest(msg: Message) {
        val messenger = msg.replyTo
        val hostToken = msg.data.getBinder(MESSAGE_KEY_HOST_TOKEN)!!
        val inputTransferToken = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.VANILLA_ICE_CREAM)
            msg.data.getParcelable(MESSAGE_KEY_INPUT_TRANSFER_TOKEN, InputTransferToken::class.java)
        else null
        val displayId = msg.data.getInt(MESSAGE_KEY_DISPLAY_ID)
        val pluginId = msg.data.getString(MESSAGE_KEY_PLUGIN_ID)!!
        val instanceId = msg.data.getInt(MESSAGE_KEY_INSTANCE_ID)
        val width = msg.data.getInt(MESSAGE_KEY_WIDTH)
        val height = msg.data.getInt(MESSAGE_KEY_HEIGHT)

        val controller = Controller(
            this,
            pluginId,
            instanceId,
            nextGuiInstanceId++,
            nextGuiSessionId++
        )
        if (controllers[instanceId] != null) {
            Log.w(LOG_TAG, "Another GUI controller for $pluginId / instanceId:$instanceId was alive. Terminating it.")
            controllers[instanceId]?.close()
        }
        controllers[instanceId] = controller
        controller.initialize(messenger, hostToken, inputTransferToken, displayId, width, height)
    }

    private fun handleGetPreferredSizeRequest(msg: Message) {
        val pluginId = msg.data.getString(MESSAGE_KEY_PLUGIN_ID)!!
        val instanceId = msg.data.getInt(MESSAGE_KEY_INSTANCE_ID)
        val preferredSize = AudioPluginServiceHelper.getNativeViewPreferredSize(this, pluginId, instanceId)
        msg.replyTo?.send(Message.obtain().apply {
            data = bundleOf(
                MESSAGE_KEY_PREFERRED_WIDTH to (preferredSize?.width ?: 0),
                MESSAGE_KEY_PREFERRED_HEIGHT to (preferredSize?.height ?: 0)
            )
        })
    }

    private fun handleDisconnectRequest(msg: Message) {
        val pluginId = msg.data.getString(MESSAGE_KEY_PLUGIN_ID) ?: ""
        val instanceId = msg.data.getInt(MESSAGE_KEY_INSTANCE_ID)
        val guiSessionId = if (msg.data.containsKey(MESSAGE_KEY_GUI_SESSION_ID))
            msg.data.getInt(MESSAGE_KEY_GUI_SESSION_ID)
        else
            msg.data.getInt(LEGACY_MESSAGE_KEY_GUI_SESSION_ID)

        val controller = controllers[instanceId]
        if (controller == null) {
            Log.w(LOG_TAG, "Ignoring disconnect request for non-existent GUI controller for $pluginId / instanceId: $instanceId (guiSessionId: $guiSessionId)")
            return
        }
        if (controller.guiSessionId != guiSessionId) {
            Log.w(
                LOG_TAG,
                "Ignoring stale disconnect request for $pluginId / instanceId: $instanceId " +
                    "(guiInstanceId: ${controller.guiInstanceId}, current guiSessionId: ${controller.guiSessionId}, requested guiSessionId: $guiSessionId)"
            )
            return
        }
        controllers.remove(instanceId)
        controller.close()
    }

    private fun handleResizeRequest(msg: Message) {
        val instanceId = msg.data.getInt(MESSAGE_KEY_INSTANCE_ID)
        val width = msg.data.getInt(MESSAGE_KEY_WIDTH)
        val height = msg.data.getInt(MESSAGE_KEY_HEIGHT)

        controllers[instanceId]?.resize(width, height)
    }

    private fun handleConfigureViewportRequest(msg: Message) {
        val instanceId = msg.data.getInt(MESSAGE_KEY_INSTANCE_ID)
        val viewportWidth = msg.data.getInt(MESSAGE_KEY_VIEWPORT_WIDTH)
        val viewportHeight = msg.data.getInt(MESSAGE_KEY_VIEWPORT_HEIGHT)
        val contentWidth = msg.data.getInt(MESSAGE_KEY_CONTENT_WIDTH)
        val contentHeight = msg.data.getInt(MESSAGE_KEY_CONTENT_HEIGHT)
        val scrollX = msg.data.getInt(MESSAGE_KEY_SCROLL_X)
        val scrollY = msg.data.getInt(MESSAGE_KEY_SCROLL_Y)

        controllers[instanceId]?.configureViewport(
            viewportWidth,
            viewportHeight,
            contentWidth,
            contentHeight,
            scrollX,
            scrollY
        )
    }

    // Represents a service-side live GUI instance plus the session that attached it
    // to a specific plugin-host surface connection.
    class Controller(
        val service: AudioPluginViewService,
        val pluginId: String,
        val instanceId: Int,
        val guiInstanceId: Int,
        val guiSessionId: Int
    ) : AutoCloseable {
        private var viewHost: SurfaceControlViewHost? = null
        private var viewportView: FrameLayout? = null
        private var pluginView: View? = null

        fun initialize(messengerToSendReply: Messenger, hostToken: IBinder, inputTransferToken: InputTransferToken?, displayId: Int, width: Int, height: Int) {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
                val display = service.getSystemService(DisplayManager::class.java)
                    .getDisplay(displayId)

                service.mainLooper.queue.addIdleHandler {
                    val newHost = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.VANILLA_ICE_CREAM && inputTransferToken != null)
                        SurfaceControlViewHost(service, display, inputTransferToken)
                    else
                        @Suppress("DEPRECATION")
                        SurfaceControlViewHost(service, display, hostToken)
                    viewHost = newHost.apply {
                        val view = AudioPluginServiceHelper.createNativeView(service, pluginId, instanceId)
                        val viewport = FrameLayout(service).apply {
                            clipChildren = true
                            clipToPadding = true
                            addView(view, FrameLayout.LayoutParams(width, height))
                        }

                        view.setViewTreeLifecycleOwner(service)
                        view.setViewTreeSavedStateRegistryOwner(service)
                        viewport.setViewTreeLifecycleOwner(service)
                        viewport.setViewTreeSavedStateRegistryOwner(service)

                        pluginView = view
                        viewportView = viewport

                        val replyMessenger = messengerToSendReply

                        fun sendContentSizeChanged(w: Int, h: Int) {
                            try {
                                replyMessenger.send(Message.obtain().apply {
                                    data = bundleOf(
                                        MESSAGE_KEY_OPCODE to OPCODE_CONTENT_SIZE_CHANGED,
                                        MESSAGE_KEY_CONTENT_WIDTH to w,
                                        MESSAGE_KEY_CONTENT_HEIGHT to h
                                    )
                                })
                            } catch (_: RemoteException) {}
                        }

                        fun registerLayoutListener(target: View) {
                            target.addOnLayoutChangeListener { _, left, top, right, bottom, oldLeft, oldTop, oldRight, oldBottom ->
                                val newW = right - left
                                val newH = bottom - top
                                if (newW != oldRight - oldLeft || newH != oldBottom - oldTop)
                                    sendContentSizeChanged(newW, newH)
                            }
                        }

                        // JUCE calls View.layout() on its peer view (first child of the container),
                        // not on the container itself, so the listener must be on that child.
                        // If the peer exists already, register directly; otherwise watch for it.
                        if (view is ViewGroup && view.childCount > 0) {
                            registerLayoutListener(view.getChildAt(0))
                        } else {
                            registerLayoutListener(view)
                            // Also watch for JUCE's peer being added asynchronously.
                            if (view is ViewGroup) {
                                view.setOnHierarchyChangeListener(object : ViewGroup.OnHierarchyChangeListener {
                                    override fun onChildViewAdded(parent: View, child: View) {
                                        if (view.indexOfChild(child) == 0) {
                                            view.setOnHierarchyChangeListener(null)
                                            registerLayoutListener(child)
                                        }
                                    }
                                    override fun onChildViewRemoved(parent: View, child: View) {}
                                })
                            }
                        }

                        // After the first complete layout, proactively report the actual content
                        // size. This handles cases where getPreferredSizeOrFallback returned a
                        // stale/default size and JUCE's peer has a different preferred size.
                        viewport.viewTreeObserver.addOnGlobalLayoutListener(object : android.view.ViewTreeObserver.OnGlobalLayoutListener {
                            override fun onGlobalLayout() {
                                if (viewport.viewTreeObserver.isAlive)
                                    viewport.viewTreeObserver.removeOnGlobalLayoutListener(this)
                                val target = if (view is ViewGroup && view.childCount > 0) view.getChildAt(0) else view
                                val w = target.width
                                val h = target.height
                                if (w > 0 && h > 0)
                                    sendContentSizeChanged(w, h)
                            }
                        })

                        setView(viewport, width, height)

                        messengerToSendReply.send(Message.obtain().apply {
                            data = bundleOf(
                                MESSAGE_KEY_GUI_SESSION_ID to guiSessionId,
                                LEGACY_MESSAGE_KEY_GUI_SESSION_ID to guiSessionId,
                                MESSAGE_KEY_SURFACE_PACKAGE to surfacePackage)
                        })
                    }
                    false
                }
            }
        }

        fun resize(width: Int, height: Int) {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
                configureViewport(width, height, width, height, 0, 0)
            }
        }

        fun configureViewport(
            viewportWidth: Int,
            viewportHeight: Int,
            contentWidth: Int,
            contentHeight: Int,
            scrollX: Int,
            scrollY: Int
        ) {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
                val view = pluginView ?: return
                viewHost?.relayout(viewportWidth, viewportHeight)

                val layoutParams = view.layoutParams
                if (layoutParams == null || layoutParams.width != contentWidth || layoutParams.height != contentHeight)
                    view.layoutParams = FrameLayout.LayoutParams(contentWidth, contentHeight)

                view.translationX = -scrollX.toFloat()
                view.translationY = -scrollY.toFloat()
                viewportView?.invalidate()
            }
        }

        override fun close() {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
                viewHost?.release()
                viewHost = null
                viewportView = null
                pluginView = null
            }
        }
    }
}
