package org.androidaudioplugin.ui.compose

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material.Button
import androidx.compose.material.Slider
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.getValue
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import org.androidaudioplugin.AudioPluginServiceInformation
import org.androidaudioplugin.PluginInformation
import org.androidaudioplugin.PortInformation
import java.nio.ByteBuffer


@Composable
fun AvailablePlugins(onItemClick: (PluginInformation) -> Unit = {}, pluginServices: List<AudioPluginServiceInformation>) {
    val small = TextStyle(fontSize = 12.sp)

    LazyColumn {
        items(pluginServices.flatMap { s -> s.plugins }) { plugin ->
            Row(
                modifier = Modifier.padding(start = 16.dp, end = 16.dp)
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

val headerModifier = Modifier.width(120.dp)

@Composable
fun Header(text: String) {
    Text(modifier = headerModifier, fontWeight = FontWeight.Bold, text = text)
}

@ExperimentalUnsignedTypes
@Composable
fun PluginDetails(plugin: PluginInformation, state: PluginListViewModel.State) {
    val scrollState = rememberScrollState(0)

    var parameters by remember { mutableStateOf(plugin.ports.map { p -> p.default }.toFloatArray()) }
    var pluginAppliedState by remember { mutableStateOf(false) }
    var waveData by remember { mutableStateOf(state.preview.inBuf) }

    Column(modifier = Modifier.padding(8.dp).verticalScroll(scrollState)) {
        Row {
            Text(text = plugin.displayName, fontSize = 20.sp)
        }
        Row {
            Header("package: ")
        }
        Row {
            Text(plugin.packageName, fontSize = 14.sp)
        }
        Row {
            Header("classname: ")
        }
        Row {
            Text(plugin.localName, fontSize = 14.sp)
        }
        if (plugin.author != null) {
            Row {
                Header("author: ")
            }
            Row {
                Text(plugin.author ?: "")
            }
        }
        if (plugin.backend != null) {
            Row {
                Header("backend: ")
            }
            Row {
                Text(plugin.backend ?: "")
            }
        }
        if (plugin.manufacturer != null) {
            Row {
                Header("manfufacturer: ")
            }
            Row {
                Text(plugin.manufacturer ?: "")
            }
        }
        Row {
            Column {
            }
        }
        WaveformDrawable(waveData = waveData)
        Row {
            Button(onClick = {
                if (!pluginAppliedState) {
                    state.preview.processAudioCompleted = {
                        waveData = state.preview.outBuf
                        pluginAppliedState = true
                        state.preview.unbindHost()
                    }
                    state.preview.applyPlugin(state.availablePluginServices.first(), plugin, parameters)
                } else {
                    waveData = state.preview.inBuf
                    pluginAppliedState = false
                }
            }) { Text(if (pluginAppliedState) "On" else "Off") }
            Button(onClick = {}) { Text("UI") }
            Button(onClick = { state.preview.playSound(pluginAppliedState) }) { Text("Play") }
        }
        Text(text = "Ports", fontSize = 20.sp, modifier = Modifier.padding(12.dp))
        Column {
            for (port in plugin.ports) {
                Row(modifier = Modifier.border(1.dp, Color.LightGray)) {
                    Column {
                        Text(
                            fontSize = 14.sp,
                            text = when (port.content) {
                                PortInformation.PORT_CONTENT_TYPE_AUDIO -> "Audio"
                                PortInformation.PORT_CONTENT_TYPE_MIDI -> "MIDI"
                                else -> "-"
                            },
                            modifier = Modifier.width(50.dp)
                        )
                        Text(
                            fontSize = 14.sp,
                            text = when (port.direction) {
                                PortInformation.PORT_DIRECTION_INPUT -> "In"
                                else -> "Out"
                            },
                            modifier = Modifier.width(30.dp)
                        )
                    }
                    Header(port.name)
                    var sliderPosition by remember { mutableStateOf(port.default) }
                    Text(
                        fontSize = 10.sp,
                        text = sliderPosition.toString(),
                        modifier = Modifier.width(40.dp).align(Alignment.CenterVertically)
                    )
                    Slider(
                        value = sliderPosition,
                        valueRange = if (port.minimum < port.maximum) port.minimum .. port.maximum else Float.MIN_VALUE..Float.MAX_VALUE,
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

@Composable
fun WaveformDrawable(waveData: ByteArray) {
    // FIXME: this is awful to some extent; it does not distinguish L/R channels. Hopefully it does not matter much.
    val floatBuffer = ByteBuffer.wrap(waveData).asFloatBuffer()
    val fa = FloatArray(waveData.size / 4)
    floatBuffer.get(fa)
    val max = fa.maxOrNull() ?: 0f
    Canvas(modifier = Modifier.fillMaxWidth().height(64.dp).border(width = 1.dp, color = Color.Gray),
        onDraw = {
            val width = this.size.width.toInt()
            val height = this.size.height
            val delta = waveData.size / 4 / width
            for (i in 0..width) {
                val fr = floatBuffer[delta * i] / max
                val h = fr * height
                drawLine(
                    Color.Black, Offset(i.toFloat(), height), Offset((i + 1).toFloat(), h)
                )
            }
        })
}
