package org.androidaudioplugin.ui.compose

import android.app.Activity
import android.content.Context
import android.content.Intent
import android.net.Uri
import android.os.Build
import android.provider.Settings
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.gestures.detectDragGestures
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.offset
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.selection.selectable
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.AlertDialog
import androidx.compose.material.Button
import androidx.compose.material.Checkbox
import androidx.compose.material.Slider
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.IntOffset
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.viewinterop.AndroidView
import kotlinx.coroutines.launch
import org.androidaudioplugin.ParameterInformation
import org.androidaudioplugin.PluginInformation
import org.androidaudioplugin.PortInformation
import org.androidaudioplugin.composeaudiocontrols.ImageStripKnob
import org.androidaudioplugin.hosting.AudioPluginInstance
import org.androidaudioplugin.hosting.AudioPluginMidiSettings
import org.androidaudioplugin.hosting.AudioPluginSurfaceControlClient
import org.androidaudioplugin.samples.host.engine.PluginPreview
import org.androidaudioplugin.ui.web.WebUIHostHelper

private val headerModifier = Modifier.width(120.dp)

@Composable
private fun ColumnHeader(text: String) {
    Text(modifier = headerModifier, fontWeight = FontWeight.Bold, text = text)
}

@Composable
fun PluginDetails(plugin: PluginInformation, viewModel: PluginListViewModel) {
    val scrollState = rememberScrollState(0)

    var pluginDetailsExpanded by remember { mutableStateOf(false) }
    var midiSettingsExpanded by remember { mutableStateOf(false) }
    var presetsExpanded by remember { mutableStateOf(false) }

    var midiSettingsFlags by remember { mutableStateOf(viewModel.preview.value.midiSettingsFlags) }

    var selectedPresetIndex by remember {
        viewModel.preview.value.selectedPresetIndex = -1 // reset before remembering
        mutableStateOf(-1)
    }
    val parameters by remember {
        mutableStateOf(viewModel.preview.value.instanceParameters.map { p -> p.defaultValue.toFloat() }
            .toFloatArray())
    }
    var pluginAppliedState by remember { mutableStateOf(false) }
    val previewState by remember { viewModel.preview }
    val waveViewSource = previewState.inBuf
    var waveState by remember { mutableStateOf(waveViewSource) }
    var pluginErrorState by remember { viewModel.errorMessage }

    //var showAlertWindowUIState by remember { mutableStateOf(false) }
    var showWebUIState by remember { mutableStateOf(false) }
    var surfaceUICreatedState by remember { mutableStateOf(false) }
    var showSurfaceUIState by remember { mutableStateOf(false) }

    if (pluginErrorState != "") {
        AlertDialog(onDismissRequest = {},
            confirmButton = {
                Button(onClick = { pluginErrorState = "" }) { Text("OK") }
            },
            title = { Text("Plugin internal error") },
            text = { Text(pluginErrorState) }
        )
    }

    Column(
        modifier = Modifier
            .padding(8.dp)
            .verticalScroll(scrollState)
    ) {
        Row {
            Text(text = (if (pluginDetailsExpanded) "[-]" else "[+]") + " details", fontSize = 20.sp, modifier = Modifier
                .padding(vertical = 12.dp)
                .clickable {
                    pluginDetailsExpanded = !pluginDetailsExpanded
                })
        }
        if (pluginDetailsExpanded) {
            Row {
                ColumnHeader("package: ")
            }
            Row {
                Text(plugin.packageName, fontSize = 14.sp)
            }
            Row {
                ColumnHeader("classname: ")
            }
            Row {
                Text(plugin.localName, fontSize = 14.sp)
            }
            if (plugin.developer != null) {
                Row {
                    ColumnHeader("developer: ")
                }
                Row {
                    Text(plugin.developer ?: "")
                }
            }
        }
        WaveformDrawable(waveData = waveState)
        Row {
            val buttonAppliedState = remember { mutableStateOf(true) }
            val coroutineScope = rememberCoroutineScope()
            Button(enabled = buttonAppliedState.value, onClick = {
                if (!pluginAppliedState) {
                    buttonAppliedState.value = false
                    previewState.processAudioCompleted = {
                        waveState = previewState.outBuf
                        pluginAppliedState = true
                        buttonAppliedState.value = true
                    }
                    coroutineScope.launch {
                        previewState.applyPlugin(parameters) {
                            pluginErrorState = it.toString()
                            buttonAppliedState.value = true
                        }
                    }
                } else {
                    buttonAppliedState.value = false
                    waveState = previewState.inBuf
                    pluginAppliedState = false
                    buttonAppliedState.value = true
                }
            }) { Text(if (pluginAppliedState) "On" else "Off") }
            // FIXME: we need better instrument detector
            Button(enabled = plugin.category?.contains("Instrument") == true,
                onClick = {
                    coroutineScope.launch { previewState.playMidiNotes() }
                }) {
                Text("MIDI")
            }
            Button(enabled = plugin.category?.contains("Instrument") == true,
                onClick = {
                    coroutineScope.launch { previewState.stopMidiService() }
                }) {
                Text("Stop MIDI")
            }
            Button(onClick = {
                coroutineScope.launch { previewState.playSound(pluginAppliedState) }
            }) {
                Text("Play")
            }
        }
        Row {
            Button(onClick = {
                showWebUIState = !showWebUIState
            }) { Text(if (showWebUIState) "Hide Web UI" else "Show Web UI") }
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
                Button(onClick = {
                    showSurfaceUIState = !showSurfaceUIState
                    if (showSurfaceUIState) {
                        if (!surfaceUICreatedState || AudioPluginSurfaceControlClient.alwaysReconnectSurfaceControl) {
                            previewState.connectSurfaceControlUI(1000, 750)
                            surfaceUICreatedState = true
                        }
                        previewState.showSurfaceGui()
                    }
                    else
                        previewState.hideSurfaceGui()
                }) { Text(if (showSurfaceUIState) "Hide Native UI" else "Show Native UI") }
            }
        }
        /*
        Row {
            Button(onClick = {
                showAlertWindowUIState = !showAlertWindowUIState
                if (showAlertWindowUIState)
                    previewState.showGui()
                else
                    previewState.hideGui()
            }) { Text(if (showAlertWindowUIState) "Hide AlertWindow UI" else "Show AlertWindow UI") }
        }
        Row {
            val context = LocalContext.current
            Button(onClick = {
                val intent = Intent(Settings.ACTION_MANAGE_OVERLAY_PERMISSION)
                intent.data = Uri.parse("package:${context.packageName}")
                if (context is Activity)
                    context.startActivityForResult(intent, 2)
            }) { Text("Grant or revoke native UI overlay permission") }
        }
        */

        Text(text = (if (midiSettingsExpanded) "[-]" else "[+]") + " Parameter MIDI mapping policy", fontSize = 20.sp, modifier = Modifier
            .padding(vertical = 12.dp)
            .clickable {
                midiSettingsExpanded = !midiSettingsExpanded
            })
        if (midiSettingsExpanded) {
            Column {
                Row {
                    Checkbox(
                        enabled = false,
                        checked = (midiSettingsFlags and AudioPluginMidiSettings.AAP_PARAMETERS_MAPPING_POLICY_PROGRAM) != 0,
                        onCheckedChange = {
                            midiSettingsFlags = midiSettingsFlags xor AudioPluginMidiSettings.AAP_PARAMETERS_MAPPING_POLICY_PROGRAM
                            viewModel.preview.value.midiSettingsFlags = midiSettingsFlags
                        })
                    Text("Consumes Program Changes by its own")
                }
                Row {
                    Checkbox(
                        enabled = false,
                        checked = (midiSettingsFlags and AudioPluginMidiSettings.AAP_PARAMETERS_MAPPING_POLICY_CC) != 0,
                        onCheckedChange = {
                            midiSettingsFlags = midiSettingsFlags xor AudioPluginMidiSettings.AAP_PARAMETERS_MAPPING_POLICY_CC
                            viewModel.preview.value.midiSettingsFlags = midiSettingsFlags
                        })
                    Text("Consumes CCs by its own")
                }
                Row {
                    Checkbox(
                        enabled = false,
                        checked = (midiSettingsFlags and AudioPluginMidiSettings.AAP_PARAMETERS_MAPPING_POLICY_ACC) != 0,
                        onCheckedChange = {
                            midiSettingsFlags = midiSettingsFlags xor AudioPluginMidiSettings.AAP_PARAMETERS_MAPPING_POLICY_ACC
                            viewModel.preview.value.midiSettingsFlags = midiSettingsFlags
                        })
                    Text("Consumes NRPNs (ACCs) by its own")
                }
                Row {
                    Checkbox(
                        enabled = false,
                        checked = (midiSettingsFlags and AudioPluginMidiSettings.AAP_PARAMETERS_MAPPING_POLICY_SYSEX8) != 0,
                        onCheckedChange = {
                            midiSettingsFlags = midiSettingsFlags xor AudioPluginMidiSettings.AAP_PARAMETERS_MAPPING_POLICY_SYSEX8
                            viewModel.preview.value.midiSettingsFlags = midiSettingsFlags
                        })
                    Text("Consumes SysEx8 by its own")
                }
            }
        }
        Text(text = "Extensions", fontSize = 20.sp, modifier = Modifier.padding(vertical = 12.dp))
        Column {
            for (extension in plugin.extensions) {
                Row(modifier = Modifier.border(1.dp, Color.LightGray)) {
                    Text(
                        (if (extension.required) "[req]" else "[opt]") + " " + (extension.uri
                            ?: "(uri unspecified)"), fontSize = 12.sp
                    )
                }
            }
        }
        Text(text = (if (presetsExpanded) "[-]" else "[+]") + " Presets", fontSize = 20.sp, modifier = Modifier
            .padding(vertical = 12.dp)
            .clickable {
                presetsExpanded = !presetsExpanded
            })
        if (presetsExpanded) {
            LazyColumn(modifier = Modifier.height(80.dp)) {
                (0 until viewModel.preview.value.presetCount).forEach { index ->
                    item {
                        Text(
                            fontSize = 14.sp,
                            fontWeight = if (index == selectedPresetIndex) FontWeight.Bold else FontWeight.Normal,
                            text = "$index: ${viewModel.preview.value.getPresetName(index)}",
                            modifier = Modifier.selectable(selected = index == selectedPresetIndex) {
                                viewModel.preview.value.selectedPresetIndex = index
                                selectedPresetIndex = index
                            })
                    }
                }
            }
        }
        Text(text = "Parameters", fontSize = 20.sp, modifier = Modifier.padding(vertical = 12.dp))
        Column {
            for (para in viewModel.preview.value.instanceParameters) {
                PluginParameterListItem(parameters, viewModel.preview.value.instanceParameters, para)
            }
        }
        Text(text = "Ports", fontSize = 20.sp, modifier = Modifier.padding(vertical = 12.dp))
        Column {
            for (port in viewModel.preview.value.instancePorts) {
                Row(modifier = Modifier.border(1.dp, Color.LightGray)) {
                    Column {
                        Text(
                            fontSize = 14.sp,
                            text = "[${port.index}] " + when (port.direction) {
                                PortInformation.PORT_DIRECTION_INPUT -> "In"
                                else -> "Out"
                            },
                            modifier = Modifier.width(60.dp)
                        )
                        Text(
                            fontSize = 14.sp,
                            text = when (port.content) {
                                PortInformation.PORT_CONTENT_TYPE_AUDIO -> "Audio"
                                PortInformation.PORT_CONTENT_TYPE_MIDI -> "MIDI"
                                PortInformation.PORT_CONTENT_TYPE_MIDI2 -> "MIDI2"
                                else -> "-"
                            },
                            modifier = Modifier.width(60.dp)
                        )
                    }
                    ColumnHeader(port.name)
                }
            }
        }
    }
    val instance = previewState.instance
    if (showWebUIState && instance != null)
        PluginWebUI(instance)
    if (showSurfaceUIState && instance != null)
        PluginSurfaceControlUI(previewState)
}

@Composable
fun PluginParameterListItem(parameters: FloatArray, instanceParameters: List<ParameterInformation>, para: ParameterInformation) {
    Row(modifier = Modifier.border(1.dp, Color.LightGray)) {
        Column {
            Text(
                fontSize = 14.sp,
                text = "${para.id}",
                modifier = Modifier.width(60.dp)
            )
        }
        ColumnHeader(para.name)
        val pi = instanceParameters.indexOfFirst { p -> para.id == p.id }
        if (pi < 0)
            return

        if (para.enumerations.any()) {
            var enumState by remember { mutableStateOf(parameters[pi]) }
            PluginParameterEnumSelector(value = enumState, enumerations = para.enumerations,
                onValueChange = {
                    parameters[pi] = it.toFloat()
                    enumState = it.toFloat()
                })
        } else {
            var sliderPosition by remember { mutableStateOf(para.defaultValue) }
            Text(fontSize = 10.sp, text = sliderPosition.toString(),
                modifier = Modifier
                    .width(40.dp)
                    .align(Alignment.CenterVertically))
            ImageStripKnob(drawableResId = R.drawable.bright_life,
                value = sliderPosition.toFloat(),
                valueRange = if (para.minimumValue < para.maximumValue) para.minimumValue.toFloat()..para.maximumValue.toFloat() else 0.0f..1.0f,
                onValueChange = {
                    parameters[pi] = it
                    sliderPosition = it.toDouble()
                })
            Slider(
                value = sliderPosition.toFloat(),
                valueRange = if (para.minimumValue < para.maximumValue) para.minimumValue.toFloat()..para.maximumValue.toFloat() else 0.0f..1.0f,
                steps = 10,
                onValueChange = {
                    parameters[pi] = it
                    sliderPosition = it.toDouble()
                })
        }
    }
}

@Composable
fun PluginSurfaceControlUI(preview: PluginPreview) {
    var offsetX by remember { mutableStateOf(0f) }
    var offsetY by remember { mutableStateOf(0f) }

    val surfaceControlClient by remember {
        mutableStateOf<AudioPluginSurfaceControlClient?>(preview.surfaceControl)
    }

    Column(
        Modifier
            .padding(10.dp)
            .offset { IntOffset(offsetX.toInt(), offsetY.toInt()) }

    ) {
        Box(Modifier
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
            .border(1.dp, Color.Black)) {
            Text(preview.pluginInfo!!.displayName)
        }
        val client = surfaceControlClient
        if (client != null)
            AndroidView(modifier = Modifier.border(1.dp, Color.Black),
                factory = { client.surfaceView }
            )
    }
}

@Composable
fun PluginWebUI(instance: AudioPluginInstance) {
    var offsetX by remember { mutableStateOf(0f) }
    var offsetY by remember { mutableStateOf(0f) }

    Column(
        Modifier
            .padding(40.dp)
            .offset { IntOffset(offsetX.toInt(), offsetY.toInt()) }

    ) {
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
            Text("Web UI")
        }
        AndroidView(
            modifier = Modifier
                .border(1.dp, Color.Black),
            factory = { ctx: Context -> WebUIHostHelper.getWebView(ctx, instance.pluginInfo.pluginId!!, instance.pluginInfo.packageName, instance.native) }
        )
    }
}
