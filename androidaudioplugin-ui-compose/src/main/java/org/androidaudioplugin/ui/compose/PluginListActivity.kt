package org.androidaudioplugin.ui.compose

import android.os.Bundle
import android.widget.Toast
import androidx.activity.compose.setContent
import androidx.appcompat.app.AppCompatActivity
import org.androidaudioplugin.hosting.AudioPluginHostHelper
import org.androidaudioplugin.PluginServiceInformation
import kotlin.system.exitProcess

open class PluginListActivity : AppCompatActivity() {

    var topAppBarText = "Plugins in this Plugin Service"

    open fun shouldListPlugin(info: PluginServiceInformation) =
        info.packageName == application.packageName

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        val services = AudioPluginHostHelper.queryAudioPluginServices(applicationContext)
            .filter { s -> shouldListPlugin(s) }
        pluginListViewModel = PluginListViewModel(applicationContext)
        pluginListViewModel.setAvailablePluginServices(services.toList())
        setContent {
            PluginListApp(topAppBarText)
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
