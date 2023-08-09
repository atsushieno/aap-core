package org.androidaudioplugin.ui.compose.app

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent

open class GenericPluginHostActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContent {
            PluginManagerTheme {
                SystemPluginManagerMain()
            }
        }
    }
}