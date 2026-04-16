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
}
