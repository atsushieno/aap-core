package org.androidaudioplugin.ui.compose.app

import android.app.Activity
import android.net.Uri
import android.widget.Toast
import androidx.activity.compose.BackHandler
import androidx.activity.compose.LocalActivity
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
    val scope by remember { mutableStateOf(
        PluginManagerScope(context,
            AudioPluginHostHelper.queryAudioPluginServices(context).toList().toMutableStateList())
    ) }
    GenericPluginManagerMain(scope, listTitleBarText = "Plugins on this system")
}

@Composable
fun LocalPluginManagerMain() {
    val context = LocalContext.current
    val scope by remember { mutableStateOf(
        PluginManagerScope(context,
            listOf(AudioPluginServiceHelper.getLocalAudioPluginService(context)).toMutableStateList())
    ) }
    GenericPluginManagerMain(scope, listTitleBarText = "Plugins in this application")
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun GenericPluginManagerMain(scope: PluginManagerScope, listTitleBarText: String) {
    var lastBackPressed by remember { mutableStateOf(System.currentTimeMillis()) }

    val navController = rememberNavController()

    NavHost(navController, startDestination = "plugin_list") {
        composable("plugin_list") {
            Scaffold(
                topBar = { TopAppBar(title = { Text(text = listTitleBarText) }) },
                content = {
                    Column(Modifier.padding(it)) {
                        PluginList(scope, onSelectItem = { pluginId ->
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
                        val pluginInfo = scope.pluginServices.flatMap { it.plugins }.first { it.pluginId == pluginId }
                        PluginDetails(pluginInfo, scope)
                    }
                }
            )
        }
    }
    val context = LocalContext.current
    BackHandler {
        if (navController.currentBackStackEntry?.destination?.route == "plugin_list") {
            if (System.currentTimeMillis() - lastBackPressed < 2000) {
                exitProcess(0)
            }
            else
                Toast.makeText(context, "Tap once more to quit", Toast.LENGTH_SHORT).show()
            lastBackPressed = System.currentTimeMillis()
        }
        else
            navController.popBackStack()
    }
}
