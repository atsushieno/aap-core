package org.androidaudioplugin.ui.compose

import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import org.androidaudioplugin.AudioPluginServiceInformation

class PluginListViewModel {
    var items by mutableStateOf(State(mutableListOf()))
        private set

    data class State(
        val availablePluginServices: MutableList<AudioPluginServiceInformation>,
    )

    fun setAvailablePluginServices(services: List<AudioPluginServiceInformation>) {
        items = State(services.toMutableList())
    }
}

val pluginListViewModel = PluginListViewModel()
