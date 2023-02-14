package org.androidaudioplugin

import android.content.Context
import android.view.View

abstract class AudioPluginViewFactory {
    abstract fun createView(context: Context, pluginId: String, instanceId: Int) : View
}
