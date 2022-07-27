package org.androidaudioplugin.ui.compose

import android.net.Uri
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.padding
import androidx.compose.material.MaterialTheme
import androidx.compose.material.Scaffold
import androidx.compose.material.Surface
import androidx.compose.material.TopAppBar
import androidx.compose.material.Text
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.em
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
        val preview by remember { viewModel.preview }
        var errorMessage by remember { mutableStateOf("") }

        NavHost(navController, startDestination="plugin_list") {
            composable("plugin_list") {
                Scaffold(
                    topBar = { TopAppBar(title = { Text(text = viewModel.topAppBarText.value) }) },
                    content = {
                        viewModel.atTopLevel.value = true // it feels ugly. There should be some better way...
                        Column {
                            if (errorMessage != "")
                                Text(errorMessage, modifier = Modifier.padding(14.dp))
                            PluginList(onItemClick = { p ->
                                if (preview.pluginInfo != null)
                                    preview.unloadPlugin()
                                val pluginId = p.pluginId
                                if (pluginId == null) {
                                    errorMessage = "pluginId is missing"
                                    return@PluginList
                                }
                                val pluginInfo = viewModel.getPluginInfo(pluginId)
                                if (pluginInfo == null) {
                                    errorMessage =
                                        "plugin $pluginId is somehow not found on the system."
                                    return@PluginList
                                }
                                preview.loadPlugin(pluginInfo) { _, error ->
                                    if (error == null)
                                        navController.navigate("plugin_details/" + Uri.encode(p.pluginId))
                                    else
                                        errorMessage = error.message ?: ""
                                }
                            }, pluginServices = viewModel.availablePluginServices)
                        }
                    }
                )
            }

            composable("plugin_details/{pluginId}",
                arguments = listOf(navArgument("pluginId") { type = NavType.StringType })) {
                viewModel.atTopLevel.value = false // it feels ugly. There should be some better way...
                Scaffold(
                    topBar = { TopAppBar(title = { Text(text = preview.pluginInfo!!.displayName) }) },
                    content = { PluginDetails(preview.pluginInfo!!, viewModel) })
            }
        }
    }
}
