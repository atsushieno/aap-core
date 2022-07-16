package org.androidaudioplugin.ui.compose

import org.androidaudioplugin.PluginServiceInformation
import org.androidaudioplugin.samples.host.engine.PluginPreview


data class PluginListModel constructor(
    var topAppBarText: String,
    val availablePluginServices: MutableList<PluginServiceInformation>,
    val preview: PluginPreview
)
