package org.androidaudioplugin.ui.compose

import androidx.compose.material.MaterialTheme
import androidx.compose.material.Scaffold
import androidx.compose.material.Surface
import androidx.compose.material.TopAppBar
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.navigation.compose.NavHost
import androidx.navigation.compose.composable
import androidx.navigation.compose.navigate
import androidx.navigation.compose.rememberNavController
import androidx.navigation.compose.navArgument

@Composable
fun PluginListApp() {
    MaterialTheme { PluginListAppContent() }
}

@Composable
fun PluginListAppContent() {
    Surface {
        val navController = rememberNavController()
        val state = pluginListViewModel.items

        NavHost(navController, startDestination="plugin_list") {
            composable("plugin_list") {
                Scaffold(
                    topBar = { TopAppBar(title = { Text(text = "Plugins in this Plugin Service") }) },
                    content = {
                        AvailablePlugins(onItemClick = { p ->
                            navController.navigate("plugin_details/" + p.pluginId)
                        }, pluginServices = state.availablePluginServices)
                    }
                )
            }

            composable("plugin_details/{pluginId}",
                arguments = listOf(navArgument("pluginId", {}))) { navBackStackEntry ->
                val pluginId = navBackStackEntry.arguments?.getString("pluginId")
                if (pluginId != null) {
                    val plugin = state.availablePluginServices.flatMap { s -> s.plugins }
                        .firstOrNull { p -> p.pluginId == pluginId }
                    if (plugin != null) {
                        Scaffold(
                            topBar = { TopAppBar(title = { Text(text = plugin.displayName) }) },
                            content = { PluginDetails(plugin) })
                    }
                }
            }
        }
    }
}

