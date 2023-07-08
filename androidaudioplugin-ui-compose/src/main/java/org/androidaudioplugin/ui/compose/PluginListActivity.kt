package org.androidaudioplugin.ui.compose

import android.Manifest
import android.content.pm.PackageManager
import android.os.Bundle
import android.provider.Settings
import android.widget.Toast
import androidx.activity.ComponentActivity
import androidx.activity.OnBackPressedCallback
import androidx.activity.compose.setContent
import org.androidaudioplugin.PluginServiceInformation
import org.androidaudioplugin.hosting.AudioPluginHostHelper
import org.androidaudioplugin.samples.host.engine.PluginPreview
import kotlin.system.exitProcess

open class PluginListActivity : ComponentActivity() {

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
        /*
        if (!Settings.canDrawOverlays(this)) {
            val info = packageManager.getPackageInfo(this.packageName, PackageManager.GET_PERMISSIONS)
            if (info.requestedPermissions != null && info.requestedPermissions.contains(Manifest.permission.SYSTEM_ALERT_WINDOW)) {
                Toast.makeText(this,
                    "Native plugin UI overlay will not work until you grant the permission.", Toast.LENGTH_SHORT)
                    .show()
            }
        }
        */
    }
}
