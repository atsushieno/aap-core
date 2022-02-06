package org.androidaudioplugin.ui.compose

import android.os.Bundle
import androidx.activity.compose.setContent
import androidx.appcompat.app.AppCompatActivity
import org.androidaudioplugin.AudioPluginHostHelper
import org.androidaudioplugin.AudioPluginServiceInformation
import org.androidaudioplugin.PluginInformation

open class PluginListActivity : AppCompatActivity() {

    open fun shouldListPlugin(info: AudioPluginServiceInformation) =
        info.packageName == application.packageName

    @ExperimentalUnsignedTypes
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        val services = AudioPluginHostHelper.queryAudioPluginServices(applicationContext)
            .filter { s -> shouldListPlugin(s) }
        pluginListViewModel = PluginListViewModel(applicationContext)
        pluginListViewModel.setAvailablePluginServices(services.toList())
        setContent {
            PluginListApp()
        }
    }
}
