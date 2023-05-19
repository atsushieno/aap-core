package org.androidaudioplugin

import android.content.Intent
import android.hardware.display.DisplayManager
import android.os.Build
import android.os.Handler
import android.os.IBinder
import android.os.Looper
import android.os.Message
import android.os.Messenger
import android.view.SurfaceControlViewHost
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
        const val OPCODE_CONNECT = 0
        const val OPCODE_DISCONNECT = 1

        // requests
        const val MESSAGE_KEY_OPCODE = "opcode"
        const val MESSAGE_KEY_HOST_TOKEN = "hostToken"
        const val MESSAGE_KEY_DISPLAY_ID = "displayId"
        const val MESSAGE_KEY_PLUGIN_ID = "pluginId"
        const val MESSAGE_KEY_INSTANCE_ID = "instanceId"
        const val MESSAGE_KEY_WIDTH = "width"
        const val MESSAGE_KEY_HEIGHT = "height"
        // replies
        const val MESSAGE_KEY_GUI_INSTANCE_ID = "guiInstanceId"
        const val MESSAGE_KEY_SURFACE_PACKAGE = "surfacePackage"
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
                    owner.handleCreateRequest(msg)
                }
                OPCODE_DISCONNECT -> {
                    owner.handleDisposeRequest(msg)
                }
                else -> {}
            }
        }
    }

    private val controllers = mutableMapOf<Int,Controller>()

    private fun handleCreateRequest(msg: Message) {
        val messenger = msg.replyTo
        val hostToken = msg.data.getBinder(MESSAGE_KEY_HOST_TOKEN)!!
        val displayId = msg.data.getInt(MESSAGE_KEY_DISPLAY_ID)
        val pluginId = msg.data.getString(MESSAGE_KEY_PLUGIN_ID)!!
        val instanceId = msg.data.getInt(MESSAGE_KEY_INSTANCE_ID)
        val width = msg.data.getInt(MESSAGE_KEY_WIDTH)
        val height = msg.data.getInt(MESSAGE_KEY_HEIGHT)

        val controller = Controller(this, pluginId, instanceId)
        if (controllers[instanceId] != null)
            throw IllegalArgumentException("Another GUI controller for $pluginId / instanceId:$instanceId is alive")
        controllers[instanceId] = controller
        controller.initialize(messenger, hostToken, displayId, width, height)
    }

    private fun handleDisposeRequest(msg: Message) {
        val pluginId = msg.data.getInt(MESSAGE_KEY_PLUGIN_ID)
        val instanceId = msg.data.getInt(MESSAGE_KEY_INSTANCE_ID)
        val guiInstanceId = msg.data.getInt(MESSAGE_KEY_GUI_INSTANCE_ID)

        val controller = controllers.remove(instanceId)
            ?: throw IllegalArgumentException("Attempt to dispose non-existent GUI controller for $pluginId / instanceId: $instanceId (guiInstanceId: $guiInstanceId)")
        controller.close()
    }

    /*
     represents a service side GUI instance that uses SurfaceControlViewHost.

     It is created per plugin GUI instance.
     */
    class Controller(
        val service: AudioPluginViewService,
        val pluginId: String,
        val instanceId: Int
    ) : AutoCloseable {
        private lateinit var viewHost: SurfaceControlViewHost

        // FIXME: there could be more than one in the future, but so far 1:1 for instance:gui.
        val guiInstanceId: Int
            get() = instanceId

        fun initialize(messengerToSendReply: Messenger, hostToken: IBinder, displayId: Int, width: Int, height: Int) {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
                val display = service.getSystemService(DisplayManager::class.java)
                    .getDisplay(displayId)

                service.mainLooper.queue.addIdleHandler {
                    viewHost = SurfaceControlViewHost(service, display, hostToken).apply {
                        val view = AudioPluginServiceHelper.createNativeView(service, pluginId, instanceId)

                        view.setViewTreeLifecycleOwner(service)
                        view.setViewTreeSavedStateRegistryOwner(service)

                        setView(view, width, height)

                        messengerToSendReply.send(Message.obtain().apply {
                            data = bundleOf(
                                MESSAGE_KEY_GUI_INSTANCE_ID to guiInstanceId,
                                MESSAGE_KEY_SURFACE_PACKAGE to surfacePackage)
                        })
                    }
                    false
                }
            }
        }

        override fun close() {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
                viewHost.release()
            }
        }
    }
}