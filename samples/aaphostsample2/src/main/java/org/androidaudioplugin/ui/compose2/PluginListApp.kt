package org.androidaudioplugin.ui.compose2

import android.annotation.SuppressLint
import android.net.Uri
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.padding
import androidx.compose.material.Scaffold
import androidx.compose.material.TopAppBar
import androidx.compose.material.Text
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import androidx.navigation.NavType
import androidx.navigation.compose.*
import androidx.navigation.navArgument

@SuppressLint("UnusedMaterialScaffoldPaddingParameter")
@Composable
fun PluginListAppContent(state: PluginListAppState) {
    var loadingStateMessage by remember { mutableStateOf("") }
    var errorMessage by remember { mutableStateOf("") }

    NavHost(state.navController, startDestination=PluginListAppState.Navigation.PluginListRoute) {
        composable(PluginListAppState.Navigation.PluginListRoute) {
            Scaffold(
                topBar = { TopAppBar(title = { Text(text = state.topAppBarText.value) }) },
                content = {
                    Column {
                        if (loadingStateMessage != "")
                            Text(loadingStateMessage, modifier = Modifier.padding(14.dp))
                        if (errorMessage != "")
                            Text(errorMessage, modifier = Modifier.padding(14.dp))
                        PluginList(state, onItemClick = { p ->
                            if (state.engine.instance != null) {
                                loadingStateMessage = "Unloading plugin ${state.engine.instance!!.pluginInfo.displayName} before reloading..."
                                state.engine.unloadPlugin()
                                loadingStateMessage = ""
                            }
                            if (p.pluginId == null) {
                                errorMessage = "pluginId is missing"
                                return@PluginList
                            }
                            loadingStateMessage = "Loading plugin ${p.displayName}..."
                            state.engine.loadPlugin(p) { instance, error ->
                                if (error != null) {
                                    errorMessage = "Failed to load plugin: ${error.message}"
                                    return@loadPlugin
                                }
                                state.engine.instance = instance
                                loadingStateMessage = ""
                                state.engine.instantiatePlugin(p.pluginId!!)
                                state.navController.navigate(PluginListAppState.Navigation.PluginDetailsRoute + "/" + Uri.encode(p.pluginId))
                            }
                        })
                    }
                }
            )
        }

        composable(PluginListAppState.Navigation.PluginDetailsRouteFormat,
            arguments = listOf(navArgument("pluginId") { type = NavType.StringType })) {
            Scaffold(
                topBar = { TopAppBar(title = { Text(text = state.engine.instance!!.pluginInfo.displayName) }) },
                content = { PluginDetails(state) })
        }
    }
}
