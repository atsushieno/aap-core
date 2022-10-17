package org.androidaudioplugin.ui.compose2

import android.os.Bundle
import android.widget.Toast
import androidx.activity.ComponentActivity
import androidx.activity.OnBackPressedCallback
import androidx.activity.compose.setContent
import androidx.compose.material.MaterialTheme
import androidx.navigation.compose.rememberNavController
import org.androidaudioplugin.hosting.AudioPluginHostHelper
import org.androidaudioplugin.PluginServiceInformation
import org.androidaudioplugin.samples.aaphostsample2.PluginHostEngine
import kotlin.system.exitProcess

open class PluginListActivity : ComponentActivity() {

    private lateinit var model: PluginHostEngine
    protected lateinit var state : PluginListAppState

    open val topAppBarText: String
        get() = "Plugins in this AAP Service"

    open fun shouldListPlugin(info: PluginServiceInformation) =
        info.packageName == application.packageName

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        val services = AudioPluginHostHelper.queryAudioPluginServices(applicationContext)
            .filter { s -> shouldListPlugin(s) }
        model = PluginHostEngine(this, services.toMutableList())
        setContent {
            MaterialTheme {
                val navController = rememberNavController()
                state = PluginListAppState(model, navController, topAppBarText)
                PluginListAppContent(state)

                onBackPressedDispatcher.addCallback(object: OnBackPressedCallback(true) {
                    private var lastBackPressed = System.currentTimeMillis()

                    override fun handleOnBackPressed() {
                        if (state.navController.currentDestination?.route != PluginListAppState.Navigation.PluginListRoute)
                            return

                        if (System.currentTimeMillis() - lastBackPressed < 2000) {
                            finish()
                            exitProcess(0)
                        }
                        else
                            Toast.makeText(this@PluginListActivity, "Tap once more to quit", Toast.LENGTH_SHORT).show()
                        lastBackPressed = System.currentTimeMillis()
                    }
                })
            }
        }
    }
}
