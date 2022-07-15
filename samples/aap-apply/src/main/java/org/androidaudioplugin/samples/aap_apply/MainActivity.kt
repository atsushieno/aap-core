package org.androidaudioplugin.samples.aap_apply

import android.content.Context
import android.net.Uri
import android.os.Bundle
import android.widget.Toast
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material.*
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.navigation.NavType
import androidx.navigation.compose.NavHost
import androidx.navigation.compose.composable
import androidx.navigation.compose.rememberNavController
import androidx.navigation.navArgument
import org.androidaudioplugin.PluginInformation
import org.androidaudioplugin.PluginServiceInformation
import org.androidaudioplugin.hosting.AudioPluginHostHelper
import kotlin.system.exitProcess

val pluginListViewModel = PluginListViewModel()

internal lateinit var appContext: Context

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        appContext = applicationContext
        setContent {
            MaterialTheme {
                val topAppBarText = "Plugins on this Android system"
                Surface {
                    val navController = rememberNavController()
                    //val state = pluginListViewModel.items

                    NavHost(navController, startDestination="plugin_list") {
                        composable("plugin_list") {
                            Scaffold(
                                topBar = { TopAppBar(title = { Text(text = topAppBarText) }) },
                                content = {
                                    pluginListViewModel.atTopLevel = true // it feels ugly. There should be some better way...
                                    PluginList(onItemClick = { p ->
                                        navController.navigate("plugin_details/" + Uri.encode(p.pluginId))
                                    }, pluginServices = AudioPluginHostHelper.queryAudioPluginServices(
                                        appContext).toList())//state.availablePluginServices)
                                }
                            )
                        }

                        composable("plugin_details/{pluginId}",
                            arguments = listOf(navArgument("pluginId") { type = NavType.StringType })) {
                            val pluginId = it.arguments?.getString("pluginId")
                            if (pluginId != null) {
                                pluginListViewModel.atTopLevel = false // it feels ugly. There should be some better way...
                                /*
                                val plugin = state.availablePluginServices.flatMap { s -> s.plugins }
                                    .firstOrNull { p -> p.pluginId == pluginId }
                                if (plugin != null) {
                                    Scaffold(
                                        topBar = { TopAppBar(title = { Text(text = plugin.displayName) }) },
                                        content = { PluginDetails(plugin, state) })
                                }
                                */
                            }
                        }
                    }
                }
            }
        }
    }

    private var lastBackPressed = System.currentTimeMillis()

    override fun onBackPressed() {
        if (!pluginListViewModel.atTopLevel) {
            super.onBackPressed()
            return
        }

        if (System.currentTimeMillis() - lastBackPressed < 2000) {
            finish()
            exitProcess(0)
        }
        else
            Toast.makeText(this, "Tap once more to quit", Toast.LENGTH_SHORT).show()
        lastBackPressed = System.currentTimeMillis()
    }
}
