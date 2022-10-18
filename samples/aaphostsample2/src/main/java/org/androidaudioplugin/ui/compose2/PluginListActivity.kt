package org.androidaudioplugin.ui.compose2

import android.os.Bundle
import android.widget.Toast
import androidx.activity.ComponentActivity
import androidx.activity.OnBackPressedCallback
import androidx.activity.compose.setContent
import androidx.compose.material.MaterialTheme
import androidx.compose.material.Surface
import androidx.navigation.compose.rememberNavController
import org.androidaudioplugin.hosting.AudioPluginHostHelper
import org.androidaudioplugin.PluginServiceInformation
import org.androidaudioplugin.hosting.AudioPluginClientBase
import org.androidaudioplugin.samples.aaphostsample2.PluginHostEngine
import kotlin.system.exitProcess

open class PluginListActivity : ComponentActivity() {

    protected lateinit var state : PluginListAppState

    open val topAppBarText: String
        get() = "Plugins in this AAP Service"

    open fun shouldListPlugin(info: PluginServiceInformation) =
        info.packageName == application.packageName

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        val services = AudioPluginHostHelper.queryAudioPluginServices(applicationContext).filter { s -> shouldListPlugin(s) }
        val engine = PluginHostEngine.create(applicationContext, services)
        setContent {
            MaterialTheme {
                val navController = rememberNavController()
                state = PluginListAppState(engine, navController, topAppBarText)

                // "tap twice to quit" implementation
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

                Surface {
                    PluginListAppContent(state)
                }
            }
        }
    }
}
