package org.androidaudioplugin.ui.compose

import android.net.Uri
import androidx.compose.material.MaterialTheme
import androidx.compose.material.Scaffold
import androidx.compose.material.Surface
import androidx.compose.material.TopAppBar
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.navigation.NavType
import androidx.navigation.compose.*
import androidx.navigation.navArgument

@Composable
fun PluginListApp(viewModel: PluginListViewModel) {
    MaterialTheme { PluginListAppContent(viewModel) }
}

@Composable
fun PluginListAppContent(viewModel: PluginListViewModel) {
    Surface {
        val navController = rememberNavController()
        val previewState by remember { viewModel.preview }

        NavHost(navController, startDestination="plugin_list") {
            composable("plugin_list") {
                Scaffold(
                    topBar = { TopAppBar(title = { Text(text = viewModel.topAppBarText.value) }) },
                    content = {
                        viewModel.atTopLevel.value = true // it feels ugly. There should be some better way...
                        PluginList(onItemClick = { p ->
                            navController.navigate("plugin_details/" + Uri.encode(p.pluginId))
                        }, pluginServices = viewModel.availablePluginServices)
                    }
                )
            }

            composable("plugin_details/{pluginId}",
                arguments = listOf(navArgument("pluginId") { type = NavType.StringType })) {
                val pluginId = it.arguments?.getString("pluginId")
                if (pluginId != null) {
                    viewModel.atTopLevel.value = false // it feels ugly. There should be some better way...
                    val plugin by remember { mutableStateOf(viewModel.availablePluginServices.flatMap { s -> s.plugins }
                        .firstOrNull { p -> p.pluginId == pluginId }) }
                    if (plugin != null) {
                        Scaffold(
                            topBar = { TopAppBar(title = { Text(text = plugin!!.displayName) }) },
                            content = { PluginDetails(plugin!!, viewModel) })
                    }
                }
            }
        }
    }
}
