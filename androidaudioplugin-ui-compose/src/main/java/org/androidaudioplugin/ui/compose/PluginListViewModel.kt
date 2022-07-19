package org.androidaudioplugin.ui.compose

import androidx.compose.runtime.getValue
import androidx.compose.runtime.setValue
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.runtime.mutableStateOf
import org.androidaudioplugin.PluginServiceInformation
import org.androidaudioplugin.samples.host.engine.PluginPreview

class PluginListViewModel(model: PluginListModel) {
    var topAppBarText = mutableStateOf(model.topAppBarText)

    val availablePluginServices = mutableStateListOf<PluginServiceInformation>()
    val preview = mutableStateOf(model.preview)
    var errorMessage = mutableStateOf(model.errorMessage)

    var atTopLevel = mutableStateOf(true)

    init {
        availablePluginServices.addAll(model.availablePluginServices)
    }
}
