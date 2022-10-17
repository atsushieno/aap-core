package org.androidaudioplugin.ui.compose2

import androidx.compose.runtime.mutableStateOf
import androidx.navigation.NavHostController
import org.androidaudioplugin.samples.aaphostsample2.PluginHostEngine

class PluginListAppState constructor(
    val engine: PluginHostEngine,
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
        engine.activatePlugin()
        activeState.value = true
    }

    fun deactivatePlugin() {
        engine.deactivatePlugin()
        activeState.value = false
    }
}
