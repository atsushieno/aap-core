package org.androidaudioplugin.ui.compose

import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material.*
import androidx.compose.runtime.*
import androidx.compose.runtime.snapshots.SnapshotStateList
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import kotlinx.coroutines.launch
import org.androidaudioplugin.PluginServiceInformation
import org.androidaudioplugin.PluginInformation
import org.androidaudioplugin.PortInformation


@Composable
fun PluginList(onItemClick: (PluginInformation) -> Unit = {}, pluginServices: SnapshotStateList<PluginServiceInformation>) {
    val small = TextStyle(fontSize = 12.sp)

    LazyColumn {
        items(pluginServices.flatMap { s -> s.plugins }) { plugin ->
            Row(
                modifier = Modifier
                    .padding(start = 16.dp, end = 16.dp)
                    .then(Modifier.clickable { onItemClick(plugin) })
            ) {
                Column(modifier = Modifier.weight(1f)) {
                    Text(plugin.displayName)
                    Text(plugin.packageName, style = small)
                }
            }
        }
    }
}

private val headerModifier = Modifier.width(120.dp)

@Composable
private fun ColumnHeader(text: String) {
    Text(modifier = headerModifier, fontWeight = FontWeight.Bold, text = text)
}

@Composable
fun PluginDetails(plugin: PluginInformation, viewModel: PluginListViewModel) {
    val scrollState = rememberScrollState(0)

    val parameters by remember { mutableStateOf(viewModel.preview.value.instancePorts.map { p -> p.default }.toFloatArray()) }
    var pluginAppliedState by remember { mutableStateOf(false) }
    val previewState by remember { viewModel.preview }
    val waveViewSource = previewState.inBuf
    var waveState by remember { mutableStateOf(waveViewSource) }
    var pluginErrorState by remember { viewModel.errorMessage }

    if (pluginErrorState != "") {
        AlertDialog(onDismissRequest = {},
            confirmButton = {
                Button(onClick = { pluginErrorState = "" }) { Text("OK") }
            },
            title = { Text("Plugin internal error") },
            text = { Text(pluginErrorState) }
        )
    }

    Column(modifier = Modifier
        .padding(8.dp)
        .verticalScroll(scrollState)) {
        Row {
            Text(text = plugin.displayName, fontSize = 20.sp)
        }
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
        if (plugin.author != null) {
            Row {
                ColumnHeader("author: ")
            }
            Row {
                Text(plugin.author ?: "")
            }
        }
        if (plugin.backend != null) {
            Row {
                ColumnHeader("backend: ")
            }
            Row {
                Text(plugin.backend ?: "")
            }
        }
        if (plugin.manufacturer != null) {
            Row {
                ColumnHeader("manufacturer: ")
            }
            Row {
                Text(plugin.manufacturer ?: "")
            }
        }
        Row {
            Column {
            }
        }
        WaveformDrawable(waveData = waveState)
        Row {
            val buttonStatePerRow = remember { mutableStateOf(true) }
            val coroutineScope = rememberCoroutineScope()
            Button(enabled = buttonStatePerRow.value, onClick = {
                if (!pluginAppliedState) {
                    buttonStatePerRow.value = false
                    previewState.processAudioCompleted = {
                        waveState = previewState.outBuf
                        pluginAppliedState = true
                        buttonStatePerRow.value = true
                    }
                    coroutineScope.launch {
                        previewState.applyPlugin(parameters) {
                            pluginErrorState = it.toString()
                            buttonStatePerRow.value = true
                        }
                    }
                } else {
                    buttonStatePerRow.value = false
                    waveState = previewState.inBuf
                    pluginAppliedState = false
                    buttonStatePerRow.value = true
                }
            }) { Text(if (pluginAppliedState) "On" else "Off") }
            Button(onClick = {}) { Text("UI") }
            Button(onClick = {
                coroutineScope.launch { previewState.playSound(pluginAppliedState) }
                }) {
                Text("Play")
            }
        }
        Text(text = "Extensions", fontSize = 20.sp, modifier = Modifier.padding(12.dp))
        Column {
            for (extension in plugin.extensions) {
                Row(modifier = Modifier.border(1.dp, Color.LightGray)) {
                    Text((if (extension.required) "[req]" else "[opt]") + " " + (extension.uri ?: "(uri unspecified)"), fontSize = 12.sp)
                }
            }
        }
        Text(text = "Ports", fontSize = 20.sp, modifier = Modifier.padding(12.dp))
        Column {
            for (port in viewModel.preview.value.instancePorts) {
                Row(modifier = Modifier.border(1.dp, Color.LightGray)) {
                    Column {
                        Text(
                            fontSize = 14.sp,
                            text = "[${port.id}] " + when (port.direction) {
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
                    var sliderPosition by remember { mutableStateOf(port.default) }
                    Text(
                        fontSize = 10.sp,
                        text = sliderPosition.toString(),
                        modifier = Modifier
                            .width(40.dp)
                            .align(Alignment.CenterVertically)
                    )
                    when (port.content) {
                        PortInformation.PORT_CONTENT_TYPE_AUDIO, PortInformation.PORT_CONTENT_TYPE_MIDI, PortInformation.PORT_CONTENT_TYPE_MIDI2 -> {}
                        else -> {
                            Slider(
                                value = sliderPosition,
                                valueRange = if (port.minimum < port.maximum) port.minimum..port.maximum else Float.MIN_VALUE..Float.MAX_VALUE,
                                steps = 10,
                                onValueChange = {
                                    parameters[plugin.ports.indexOf(port)] = it
                                    sliderPosition = it
                                })
                        }
                    }
                }
            }
        }
    }
}
