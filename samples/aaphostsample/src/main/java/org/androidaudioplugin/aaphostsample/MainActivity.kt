package org.androidaudioplugin.aaphostsample

import android.os.Bundle
import org.androidaudioplugin.ui.compose.PluginListActivity
import org.androidaudioplugin.*


class MainActivity : PluginListActivity() {
    override fun shouldListPlugin(info: PluginServiceInformation) = true

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        viewModel.topAppBarText.value = "Plugins on this device"
    }
}
