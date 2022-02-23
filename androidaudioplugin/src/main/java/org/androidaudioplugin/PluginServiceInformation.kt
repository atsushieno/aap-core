package org.androidaudioplugin

import android.graphics.drawable.Drawable

/**
 * Plugin Service information structure. The members mostly correspond to `<service>` element
 * and `<meta-data>` for an AudioPluginService in `AndroidManifest.xml`
 */
class PluginServiceInformation(var label: String, var packageName: String, var className: String,
                               var icon: Drawable? = null) {
    var extensions = mutableListOf<String>()
    var plugins = mutableListOf<PluginInformation>()
}
