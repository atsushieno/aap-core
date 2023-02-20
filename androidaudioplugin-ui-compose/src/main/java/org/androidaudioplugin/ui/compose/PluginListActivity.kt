package org.androidaudioplugin.ui.compose

import android.Manifest
import android.content.Intent
import android.content.pm.PackageManager
import android.net.Uri
import android.os.Bundle
import android.provider.Settings
import android.util.Log
import android.widget.Toast
import androidx.activity.ComponentActivity
import androidx.activity.OnBackPressedCallback
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
import androidx.activity.result.registerForActivityResult
import androidx.appcompat.app.AppCompatActivity
import androidx.compose.material.AlertDialog
import androidx.compose.material.Text
import androidx.compose.material.TextButton
import androidx.compose.ui.window.DialogProperties
import androidx.core.content.ContextCompat
import org.androidaudioplugin.hosting.AudioPluginHostHelper
import org.androidaudioplugin.PluginServiceInformation
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

            if (!Settings.canDrawOverlays(this)) {
                val info = packageManager.getPackageInfo(this.packageName, PackageManager.GET_PERMISSIONS)
                if (info.requestedPermissions != null && info.requestedPermissions.contains(Manifest.permission.SYSTEM_ALERT_WINDOW)) {
                    val intent = Intent(Settings.ACTION_MANAGE_OVERLAY_PERMISSION)
                    intent.data = Uri.parse("package:$packageName")
                    startActivityForResult(intent, 2)
                }
            }
        }
    }

    init {
        onBackPressedDispatcher.addCallback(object: OnBackPressedCallback(true) {
            private var lastBackPressed = System.currentTimeMillis()

            override fun handleOnBackPressed() {
                if (!viewModel.atTopLevel.value)
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
