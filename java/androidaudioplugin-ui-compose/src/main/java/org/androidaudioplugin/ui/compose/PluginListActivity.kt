package org.androidaudioplugin.ui.compose

import android.os.Bundle
import androidx.activity.compose.setContent
import androidx.appcompat.app.AppCompatActivity
import org.androidaudioplugin.AudioPluginHostHelper

class PluginListActivity : AppCompatActivity() {
    @ExperimentalUnsignedTypes
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        val services = AudioPluginHostHelper.queryAudioPluginServices(applicationContext)
            .filter { s -> s.packageName == application.packageName }
        pluginListViewModel = PluginListViewModel(applicationContext)
        pluginListViewModel.setAvailablePluginServices(services.toList())
        setContent {
            PluginListApp()
        }
    }
}
