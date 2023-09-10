package org.androidaudioplugin.ui.compose.app

import android.Manifest
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
import com.google.accompanist.permissions.ExperimentalPermissionsApi
import com.google.accompanist.permissions.isGranted
import com.google.accompanist.permissions.rememberPermissionState
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
import org.androidaudioplugin.ui.compose.PluginViewScopePreset
import org.androidaudioplugin.ui.compose.PluginViewScopePresetImpl
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

    override fun getParameter(parameterIndex: Int): PluginViewScopeParameter =
        PluginViewScopeParameterImpl(parameterIndex, instance.getParameter(parameterIndex), parameters[parameterIndex] ?: 0.0)

    override val portCount: Int
        get() = instance.getPortCount()

    override fun getPort(index: Int): PluginViewScopePort = PluginViewScopePortImpl(instance.getPort(index))
    override val presetCount: Int
        get() = instance.getPresetCount()

    override fun getPreset(index: Int): PluginViewScopePreset = PluginViewScopePresetImpl(index, instance.getPresetName(index))
}

@OptIn(ExperimentalPermissionsApi::class)
@Composable
fun PluginInstanceControl(scope: PluginDetailsScope,
                          pluginInfo: PluginInformation,
                          instance: NativeRemotePluginInstance,
) {
    MidiSettings(midiSettingsFlags = instance.getMidiMappingPolicy(),
        midiSeetingsFlagsChanged = { newFlags ->
            scope.setNewMidiMappingFlags(
                pluginInfo.pluginId!!,
                newFlags
            )
        })

    Row {
        var isActivated by remember { mutableStateOf(false) }
        Button(onClick = {
            isActivated = !isActivated
            if (isActivated)
                scope.startProcessing()
            else
                scope.pauseProcessing()
        }) {
            Text(text = if (isActivated) "Pause" else "Start")
        }
        Button(onClick = { scope.playPreloadedAudio() }) {
            Text(text = "Play Audio")
        }

        val permState = rememberPermissionState(Manifest.permission.RECORD_AUDIO)
        if (permState.status.isGranted)
            scope.enableAudioRecorder()
        else {
            Button(onClick = { permState.launchPermissionRequest() }) {
                Text("enable Audio Recording")
            }
        }
    }

    // FIXME: should this be hoisted out? It feels like performance loss
    val parameters = remember { (0 until instance.getParameterCount())
        .map { instance.getParameter(it).defaultValue }
        .toMutableStateList() }
    val pluginViewScope by remember { mutableStateOf(RemotePluginViewScopeImpl(scope.manager.context, instance, parameters, pluginInfo)) }
    pluginViewScope.PluginView(getParameterValue = { parameters[it].toFloat() },
        onParameterChange = { index, value ->
            parameters[index] = value.toDouble()
            scope.setParameterValue(index, value)
                            },
        onPresetChange = { index -> scope.setPresetIndex(index) },
        onNoteOn = { note, _ -> scope.setNoteState(note, true) },
        onNoteOff = { note, _ ->  scope.setNoteState(note, false) },
        onExpression = {origin, note, value -> scope.processExpression(origin, note, value) })
}
