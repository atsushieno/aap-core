package org.androidaudioplugin

import android.content.Context
import android.util.Size
import android.view.View

abstract class AudioPluginViewFactory {
    /**
     * Returns the plugin UI content's preferred Android pixel size before host decoration.
     */
    open fun getPreferredSize(context: Context, pluginId: String, instanceId: Int) : Size? = null
    abstract fun createView(context: Context, pluginId: String, instanceId: Int) : View

    /**
     * Called when the UI session that [view] was created for (by [createView]) is closed and the
     * view is never shown again. Release what the view holds (e.g. the native editor) here.
     * It does nothing by default.
     */
    open fun maybeDestroyView(context: Context, pluginId: String, instanceId: Int, view: View) {}
}
