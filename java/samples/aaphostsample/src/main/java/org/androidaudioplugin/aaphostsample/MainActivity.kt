package org.androidaudioplugin.aaphostsample

import org.androidaudioplugin.ui.compose.PluginListActivity
import org.androidaudioplugin.*

class MainActivity() : PluginListActivity() {
    override fun shouldListPlugin(info: AudioPluginServiceInformation) = true
}
