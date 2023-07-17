package org.androidaudioplugin.ui.compose.app

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.material3.Surface

class PluginManagerActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContent {
            PluginManagerTheme {
                Surface {
                    LocalPluginManagerMain()
                }
            }
        }
    }
}