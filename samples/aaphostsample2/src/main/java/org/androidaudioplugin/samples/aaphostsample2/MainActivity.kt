package org.androidaudioplugin.samples.aaphostsample2

import android.os.Bundle
import org.androidaudioplugin.ui.compose2.PluginListActivity
import org.androidaudioplugin.*

/*
    The Compose part of the app is designed to not depend on Android specifics such as
    ViewModel and LiveData.
 */
class MainActivity : PluginListActivity() {
    override fun shouldListPlugin(info: PluginServiceInformation) = true

    override val topAppBarText: String
        get() = "Plugins on this device"
}
