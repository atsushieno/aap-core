package org.androidaudioplugin.ui.compose

import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.lifecycle.MutableLiveData
import org.androidaudioplugin.AudioPluginServiceInformation
import org.androidaudioplugin.PluginInformation

class PluginListViewModel {
    var items by mutableStateOf(State(ModalPanelState.None, mutableListOf(), null))
        private set

    data class State(
        val modalState: ModalPanelState,
        val availablePluginServices: MutableList<AudioPluginServiceInformation>,
        val selectedPluginDetails: PluginInformation?
    )

    fun setAvailablePluginServices(services: List<AudioPluginServiceInformation>) {
        items = State(items.modalState,  services.toMutableList(), items.selectedPluginDetails)
    }

    fun onModalStateChanged(newState: ModalPanelState) {
        items = State(newState, items.availablePluginServices ?: mutableListOf(), items.selectedPluginDetails)
    }

    fun onSelectedPluginDetailsChanged(plugin: PluginInformation?) {
        items = State(items.modalState,  items.availablePluginServices ?: mutableListOf(), plugin)
    }
}

val pluginListViewModel = PluginListViewModel()
