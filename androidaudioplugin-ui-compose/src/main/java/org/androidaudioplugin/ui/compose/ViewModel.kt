package org.androidaudioplugin.ui.compose

import android.content.Context
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import org.androidaudioplugin.PluginServiceInformation
import org.androidaudioplugin.samples.host.engine.PluginPreview

class PluginListViewModel(ctx: Context) {
    @ExperimentalUnsignedTypes
    var items by mutableStateOf(State(mutableListOf(), PluginPreview(ctx)))
        private set

    data class State @ExperimentalUnsignedTypes constructor(
        val availablePluginServices: MutableList<PluginServiceInformation>,
        val preview: PluginPreview
    )

    @ExperimentalUnsignedTypes
    fun setAvailablePluginServices(services: List<PluginServiceInformation>) {
        items = State(services.toMutableList(), items.preview)
    }
}

@ExperimentalUnsignedTypes
lateinit var pluginListViewModel : PluginListViewModel
