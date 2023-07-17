package org.androidaudioplugin.ui.compose.app

import android.app.Activity
import android.net.Uri
import android.widget.Toast
import androidx.activity.compose.BackHandler
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TopAppBar
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.runtime.toMutableStateList
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.tooling.preview.Preview
import androidx.navigation.NavType
import androidx.navigation.compose.NavHost
import androidx.navigation.compose.composable
import androidx.navigation.compose.rememberNavController
import androidx.navigation.navArgument
import org.androidaudioplugin.AudioPluginServiceHelper
import org.androidaudioplugin.hosting.AudioPluginHostHelper
import kotlin.system.exitProcess

@Preview
@Composable
fun GenericPluginHostPreview() {
    PluginManagerTheme {
        Surface {
            SystemPluginManagerMain()
        }
    }
}

@Composable
fun SystemPluginManagerMain() {
    val context = LocalContext.current
    val data by remember { mutableStateOf(
        PluginManagerContext(context,
            AudioPluginHostHelper.queryAudioPluginServices(context).toList().toMutableStateList())
    ) }
    GenericPluginManagerMain(context = data,
        listTitleBarText = "Plugins on this system")
}

@Composable
fun LocalPluginManagerMain() {
    val context = LocalContext.current
    val data by remember { mutableStateOf(
        PluginManagerContext(context,
            listOf(AudioPluginServiceHelper.getLocalAudioPluginService(context)).toMutableStateList())
    ) }
    GenericPluginManagerMain(context = data, listTitleBarText = "Plugins in this application")

    LaunchedEffect(key1 = "") {
        data.client.connectToPluginService(data.pluginServices.first().packageName)
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun GenericPluginManagerMain(context: PluginManagerContext, listTitleBarText: String) {
    var lastBackPressed by remember { mutableStateOf(System.currentTimeMillis()) }

    val navController = rememberNavController()

    NavHost(navController, startDestination = "plugin_list") {
        composable("plugin_list") {
            Scaffold(
                topBar = { TopAppBar(title = { Text(text = listTitleBarText) }) },
                content = {
                    Column(Modifier.padding(it)) {
                        PluginList(context, onSelectItem = { pluginId ->
                            navController.navigate("plugin_details/${Uri.encode(pluginId)}")
                        })
                    }
                }
            )
        }

        composable("plugin_details/{pluginId}",
            arguments = listOf(navArgument("pluginId") { type = NavType.StringType })
        ) { entry ->
            Scaffold(
                topBar = { TopAppBar(title = { Text(text = "Plugins") }) },
                content = {
                    Column(
                        Modifier
                            .padding(it)
                            .verticalScroll(rememberScrollState())) {
                        val pluginId = Uri.decode(entry.arguments!!.getString("pluginId"))
                        val pluginInfo = context.pluginServices.flatMap { it.plugins }.first { it.pluginId == pluginId }
                        PluginDetails(context, pluginInfo)
                    }
                }
            )
        }
    }
    val activity = LocalContext.current as Activity?
    BackHandler {
        if (activity != null && navController.currentBackStackEntry?.destination?.route == "plugin_list") {
            if (System.currentTimeMillis() - lastBackPressed < 2000) {
                activity.finish()
                exitProcess(0)
            }
            else
                Toast.makeText(activity, "Tap once more to quit", Toast.LENGTH_SHORT).show()
            lastBackPressed = System.currentTimeMillis()
        }
        else
            navController.popBackStack()
    }
}
