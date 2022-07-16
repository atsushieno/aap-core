package org.androidaudioplugin.aaphostsample

import org.androidaudioplugin.ui.compose.PluginListActivity
import org.androidaudioplugin.*

class MainActivity : PluginListActivity() {
    override fun shouldListPlugin(info: PluginServiceInformation) = true

    init {
        viewModel.topAppBarText = "Plugins on this device"
    }
}
