package org.androidaudioplugin.ui.compose.app

import android.content.Context
import android.os.Build
import android.util.Log
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
import androidx.compose.material3.Button
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.runtime.toMutableStateList
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.unit.IntOffset
import androidx.compose.ui.unit.dp
import androidx.compose.ui.viewinterop.AndroidView
import kotlinx.coroutines.GlobalScope
import kotlinx.coroutines.launch
import org.androidaudioplugin.PluginInformation
import org.androidaudioplugin.hosting.NativeRemotePluginInstance
import org.androidaudioplugin.ui.compose.PluginView
import org.androidaudioplugin.ui.compose.PluginViewScope
import org.androidaudioplugin.ui.compose.PluginViewScopeParameter
import org.androidaudioplugin.ui.compose.PluginViewScopeParameterImpl
import org.androidaudioplugin.ui.compose.PluginViewScopePort
import org.androidaudioplugin.ui.compose.PluginViewScopePortImpl
import org.androidaudioplugin.ui.web.WebUIHostHelper

internal class RemotePluginViewScopeImpl(
    val context: Context,
    val instance: NativeRemotePluginInstance,
    val parameters: List<Double>,
    val pluginInfo: PluginInformation)
    : PluginViewScope {

    override val pluginName: String
        get() = pluginInfo.displayName

    override val parameterCount: Int
        get() = instance.getParameterCount()

    override fun getParameter(index: Int): PluginViewScopeParameter = PluginViewScopeParameterImpl(instance.getParameter(index), parameters[index])

    override val portCount: Int
        get() = instance.getPortCount()

    override fun getPort(index: Int): PluginViewScopePort = PluginViewScopePortImpl(instance.getPort(index))
}

@Composable
fun PluginInstanceControl(scope: PluginDetailsScope,
                          pluginInfo: PluginInformation,
                          instance: NativeRemotePluginInstance,
                          webUIVisible: Boolean = false,
                          webUIVisibleChanged: (Boolean) -> Unit = {},
                          surfaceControlUIVisible: Boolean = false,
                          surfaceControlUIVisibleChanged: (Boolean) -> Unit = {}
) {
    MidiSettings(midiSettingsFlags = instance.getMidiMappingPolicy(),
        midiSeetingsFlagsChanged = { newFlags ->
            scope.setNewMidiMappingFlags(
                pluginInfo.pluginId!!,
                newFlags
            )
        })

    TextButton(onClick = { scope.playPreloadedAudio() }) {
        Text(text = "Play Audio")
    }

    Row {
        Button(onClick = {
            webUIVisibleChanged(!webUIVisible)
        }) { Text(if (webUIVisible) "Hide Web UI" else "Show Web UI") }
        Button(onClick = {
            surfaceControlUIVisibleChanged(!surfaceControlUIVisible)
        }) { Text(if (surfaceControlUIVisible) "Hide Native UI" else "Show Native UI") }
    }

    // FIXME: should this be hoisted out? It feels like performance loss
    val parameters = remember { (0 until instance.getParameterCount())
        .map { instance.getParameter(it).defaultValue }
        .toMutableStateList() }
    val pluginViewScope by remember { mutableStateOf(RemotePluginViewScopeImpl(scope.manager.context, instance, parameters, pluginInfo)) }
    pluginViewScope.PluginView(getParameterValue = { parameters[it].toFloat() },
        onParameterChange = { index, value ->
            parameters[index] = value.toDouble()
            scope.setParameterValue(index.toUInt(), value)
                            },
        onNoteOn = { note, _ -> scope.setNoteState(note, true) },
        onNoteOff = { note, _ ->  scope.setNoteState(note, false) },
        onExpression = {origin, note, value -> scope.processExpression(origin, note, value) })
}

@Composable
fun PluginSurfaceControlUI(scope: PluginDetailsScope,
                           pluginInfo: PluginInformation,
                           instanceId: Int,
                           width: Int,
                           height: Int,
                           visible: Boolean,
                           onCloseClick: () -> Unit = {}) {
    var offsetX by remember { mutableStateOf(0f) }
    var offsetY by remember { mutableStateOf(0f) }
    var notifyConnected by remember { mutableStateOf(false) }

    if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R)
        return

    val surfaceControl = scope.surfaceControl

    if (visible) {
        surfaceControl.surfaceView.visibility = View.VISIBLE
    } else {
        surfaceControl.surfaceView.visibility = View.GONE
    }

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
        var connectionStarted by remember { mutableStateOf(false) }
        AndroidView(modifier = Modifier.border(1.dp, Color.Black),
            factory = { surfaceControl.surfaceView },
            update = {
                if (connectionStarted)
                    return@AndroidView
                connectionStarted = true
                scope.surfaceControl.connectedListeners.add {
                    // FIXME: I could not manage to get this SurfaceView shown at rendered state at first.
                    // It completes to setSurfaceChildPackage() as it is logged, but seems to some more means to forcibly update.
                    // The code below is therefore not working.
                    //
                    // This triggers updating a remembered state that only prints some string to log.
                    // It is important to trigger updates to the Composable to update AndroidView
                    // that contains this SurfaceView.
                    notifyConnected = true
                    scope.manager.context.mainLooper.queue.addIdleHandler {
                        scope.surfaceControl.surfaceView.layout(0, 0, 300, 300)
                        scope.surfaceControl.surfaceView.requestLayout()
                        scope.surfaceControl.surfaceView.z = -1f
                        false
                    }
                }
                GlobalScope.launch {
                    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R)
                        scope.surfaceControl.connectUI(pluginInfo.packageName, pluginInfo.pluginId!!, instanceId, width, height)
                }
            }
        )
        if (notifyConnected) {
            Log.d(scope.manager.logTag, "${pluginInfo.displayName}: Remote Native UI connected")
            notifyConnected = false
            Text("Remote Native UI connected")
        }
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

