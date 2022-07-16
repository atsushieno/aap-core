package org.androidaudioplugin.ui.compose

import androidx.compose.runtime.getValue
import androidx.compose.runtime.setValue
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.runtime.mutableStateOf
import org.androidaudioplugin.PluginServiceInformation

class PluginListViewModel(model: PluginListModel) {
    var topAppBarText by mutableStateOf(model.topAppBarText)

    val availablePluginServices = mutableStateListOf<PluginServiceInformation>()
    val preview by mutableStateOf(model.preview)

    var atTopLevel = mutableStateOf(true)

    init {
        availablePluginServices.addAll(model.availablePluginServices)
    }
}
