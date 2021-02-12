package org.androidaudioplugin.ui.compose

import com.arkivanov.decompose.value.MutableValue
import com.arkivanov.decompose.value.Value
import com.arkivanov.decompose.value.reduce
import org.androidaudioplugin.AudioPluginServiceInformation
import org.androidaudioplugin.PluginInformation

class PluginListViewModel {
    private val _value = MutableValue(State(ModalPanelState.None,
        mutableListOf(), null))

    val state: Value<State> = _value

    data class State(
        val modalState: ModalPanelState,
        val availablePluginServices: MutableList<AudioPluginServiceInformation>,
        val selectedPluginDetails: PluginInformation?
    )

    fun setAvailablePluginServices(services: List<AudioPluginServiceInformation>) {
        _value.reduce { it.copy(availablePluginServices = services.toMutableList()) }
    }

    fun onModalStateChanged(newState: ModalPanelState) {
        _value.reduce { it.copy(modalState = newState) }
    }

    fun onSelectedPluginDetailsChanged(plugin: PluginInformation?) {
        _value.reduce { it.copy(selectedPluginDetails = plugin) }
    }
}

val pluginListViewModel = PluginListViewModel()
