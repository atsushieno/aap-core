package org.androidaudioplugin.ui.compose.app

import android.content.Context
import android.view.SurfaceView
import android.view.View
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.gestures.detectDragGestures
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.offset
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.ExperimentalComposeUiApi
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.unit.IntOffset
import androidx.compose.ui.unit.dp
import androidx.compose.ui.viewinterop.AndroidView
import org.androidaudioplugin.PluginInformation
import org.androidaudioplugin.hosting.NativeRemotePluginInstance
import org.androidaudioplugin.ui.web.WebUIHostHelper

@OptIn(ExperimentalComposeUiApi::class)
@Composable
fun PluginSurfaceControlUI(pluginInfo: PluginInformation,
                           createSurfaceView: (Context) -> View,
                           detachSurfaceView: (View) -> Unit,
                           onCloseClick: () -> Unit = {}) {
    var offsetX by remember { mutableStateOf(0f) }
    var offsetY by remember { mutableStateOf(0f) }

    Column(
        Modifier
            .padding(10.dp)
            .offset { IntOffset(offsetX.toInt(), offsetY.toInt()) }

    ) {
        // title bar
        Row(Modifier
            .padding(0.dp)
            .pointerInput(Unit) {
                detectDragGestures { change, dragAmount ->
                    change.consume()
                    offsetX += dragAmount.x
                    offsetY += dragAmount.y
                }
            }
            .fillMaxWidth()
            .background(Color.DarkGray.copy(alpha = 0.5f))
            .border(1.dp, Color.Black)
        ) {
            TextButton(onClick = { onCloseClick() }) {
                Text("[X]")
            }
            Text(pluginInfo.displayName)
        }
        AndroidView(modifier = Modifier.border(1.dp, Color.Black),
            factory = { ctx -> createSurfaceView(ctx) },
            onRelease = { view: View -> detachSurfaceView(view) },
            onReset = { _: View -> }
        )
    }
}

@Composable
fun PluginWebUI(packageName: String, pluginId: String, native: NativeRemotePluginInstance,
                onCloseClick: () -> Unit = {}) {
    var offsetX by remember { mutableStateOf(0f) }
    var offsetY by remember { mutableStateOf(0f) }

    Column(
        Modifier
            .padding(40.dp)
            .offset { IntOffset(offsetX.toInt(), offsetY.toInt()) }) {
        // Titlebar
        Box(Modifier
            .pointerInput(Unit) {
                detectDragGestures { change, dragAmount ->
                    change.consume()
                    offsetX += dragAmount.x
                    offsetY += dragAmount.y
                }
            }
            .fillMaxWidth()
            .background(Color.DarkGray.copy(alpha = 0.3f))
            .border(1.dp, Color.Black)) {
            Row {
                TextButton(onClick = { onCloseClick() }) {
                    Text("[X]")
                }
                Text("Web UI")
            }
        }
        AndroidView(
            modifier = Modifier.border(1.dp, Color.Black),
            factory = { ctx: Context -> WebUIHostHelper.getWebView(ctx, pluginId, packageName, native) }
        )
    }
}

