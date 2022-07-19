package org.androidaudioplugin.ui.compose

import android.os.Bundle
import android.widget.Toast
import androidx.activity.compose.setContent
import androidx.appcompat.app.AppCompatActivity
import org.androidaudioplugin.hosting.AudioPluginHostHelper
import org.androidaudioplugin.PluginServiceInformation
import org.androidaudioplugin.samples.host.engine.PluginPreview
import kotlin.system.exitProcess

open class PluginListActivity : AppCompatActivity() {

    protected lateinit var model: PluginListModel
    protected lateinit var viewModel : PluginListViewModel

    open fun shouldListPlugin(info: PluginServiceInformation) =
        info.packageName == application.packageName

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        val services = AudioPluginHostHelper.queryAudioPluginServices(applicationContext)
            .filter { s -> shouldListPlugin(s) }
        model = PluginListModel("Plugins in this AAP Service", services.toMutableList(), PluginPreview(applicationContext), "")
        viewModel = PluginListViewModel(model)
        setContent {
            PluginListApp(viewModel)
        }
    }

    private var lastBackPressed = System.currentTimeMillis()

    override fun onBackPressed() {
        if (!viewModel.atTopLevel.value) {
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
