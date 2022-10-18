package org.androidaudioplugin.ui.compose2

import androidx.compose.foundation.border
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.AlertDialog
import androidx.compose.material.Button
import androidx.compose.material.Slider
import androidx.compose.material.Text
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import org.androidaudioplugin.PortInformation
import org.androidaudioplugin.hosting.AudioPluginInstance

@Composable
fun PluginDetails(state: PluginListAppState) {
    val scrollState = rememberScrollState(0)

    val plugin = state.engine.instance!!

    val parameters by remember {
        mutableStateOf((0 until plugin.getParameterCount())
            .map { i -> plugin.getParameter(i) }
            .map { p -> p.defaultValue.toFloat() }.toFloatArray()
        )
    }
    var pluginAppliedState by remember { mutableStateOf(false) }
    var pluginErrorState by remember { state.errorMessage }

    val columnHeader = @Composable { text: String ->
        Text(
            modifier = Modifier.width(120.dp),
            fontWeight = FontWeight.Bold,
            text = text
        )
    }

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
            Text(text = plugin.pluginInfo.displayName, fontSize = 20.sp)
        }
        Row {
            columnHeader("package: ")
        }
        Row {
            Text(plugin.pluginInfo.packageName, fontSize = 14.sp)
        }
        Row {
            columnHeader("classname: ")
        }
        Row {
            Text(plugin.pluginInfo.localName, fontSize = 14.sp)
        }
        if (plugin.pluginInfo.author != null) {
            Row {
                columnHeader("author: ")
            }
            Row {
                Text(plugin.pluginInfo.author ?: "")
            }
        }
        if (plugin.pluginInfo.manufacturer != null) {
            Row {
                columnHeader("manufacturer: ")
            }
            Row {
                Text(plugin.pluginInfo.manufacturer ?: "")
            }
        }

        //WaveformDrawable(waveData = waveState)

        Row {
            val activeState = remember { state.activeState }
            Button(onClick = {
                val instance = state.engine.instance!!
                if (activeState.value)
                    state.deactivatePlugin()
                else if (instance.state != AudioPluginInstance.InstanceState.ERROR)
                    state.activatePlugin()
            }) {
                Text(if (activeState.value) "Deactivate" else "Activate")
            }
            Button(onClick = {
                state.engine.play()
            }) {
                Text("Play")
            }
        }
        Text(text = "Extensions", fontSize = 20.sp, modifier = Modifier.padding(12.dp))
        Column {
            for (extension in plugin.pluginInfo.extensions) {
                Row(modifier = Modifier.border(1.dp, Color.LightGray)) {
                    Text(
                        (if (extension.required) "[req]" else "[opt]") + " " + (extension.uri
                            ?: "(uri unspecified)"), fontSize = 12.sp
                    )
                }
            }
        }
        Text(text = "Parameters", fontSize = 20.sp, modifier = Modifier.padding(12.dp))
        Column {
            (0 until plugin.getParameterCount()).forEach { i ->
                val para = plugin.getParameter(i)
                Row(modifier = Modifier.border(1.dp, Color.LightGray)) {
                    Column {
                        Text(
                            fontSize = 14.sp,
                            text = "${para.id}",
                            modifier = Modifier.width(60.dp)
                        )
                    }
                    columnHeader(para.name)
                    var sliderPosition by remember { mutableStateOf(para.defaultValue) }
                    Text(
                        fontSize = 10.sp,
                        text = sliderPosition.toString(),
                        modifier = Modifier
                            .width(40.dp)
                            .align(Alignment.CenterVertically)
                    )
                    Slider(
                        value = sliderPosition.toFloat(),
                        valueRange = if (para.minimumValue < para.maximumValue) para.minimumValue.toFloat()..para.maximumValue.toFloat() else 0.0f..1.0f,
                        steps = 10,
                        onValueChange = {
                            parameters[i] = it
                            sliderPosition = it.toDouble()
                        })
                }
            }
        }
        Text(text = "Ports", fontSize = 20.sp, modifier = Modifier.padding(12.dp))
        Column {
            (0 until plugin.getPortCount()).map { i ->
                val port = plugin.getPort(i)
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
                    columnHeader(port.name)
                }
            }
        }
    }
}