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
    /** The process the service runs in (`ServiceInfo.processName`), if known. */
    var processName: String? = null
    /**
     * The `AudioPluginViewService` class that hosts native plugin UI for this service, given by
     * the `#ViewService` meta-data. `null` means the stock `AudioPluginViewService`.
     */
    var viewServiceClassName: String? = null
}
