package org.androidaudioplugin.ui.compose

import android.os.Bundle
import androidx.activity.compose.setContent
import androidx.appcompat.app.AppCompatActivity
import org.androidaudioplugin.AudioPluginHostHelper

class PluginListActivity : AppCompatActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        val services = AudioPluginHostHelper.queryAudioPluginServices(applicationContext)
        pluginListViewModel.setAvailablePluginServices(services.toList())
        setContent {
            PluginListApp()
        }
    }
}
