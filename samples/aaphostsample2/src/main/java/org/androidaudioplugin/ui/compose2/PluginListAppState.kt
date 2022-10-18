package org.androidaudioplugin.ui.compose2

import androidx.compose.runtime.mutableStateOf
import androidx.navigation.NavHostController
import org.androidaudioplugin.samples.aaphostsample2.PluginHostEngine

class PluginListAppState constructor(
    var engine: PluginHostEngine,
    val navController: NavHostController,
    topAppBarText: String = "Plugins on this device"
) {
    object Navigation {
        const val PluginListRoute = "plugin_list"
        const val PluginDetailsRoute = "plugin_details"
        const val PluginDetailsRouteFormat = "plugin_details/{pluginId}"
    }

    val activeState = mutableStateOf(false)
    var topAppBarText = mutableStateOf(topAppBarText)
    var errorMessage = mutableStateOf("")

    fun activatePlugin() {
        val engine = this.engine ?: return
        engine.activatePlugin()
        activeState.value = true
    }

    fun deactivatePlugin() {
        val engine = this.engine ?: return
        engine.deactivatePlugin()
        activeState.value = false
    }
}
