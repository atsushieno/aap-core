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
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import kotlinx.coroutines.GlobalScope
import kotlinx.coroutines.MainScope
import kotlinx.coroutines.coroutineScope
import kotlinx.coroutines.launch
import org.androidaudioplugin.AudioPluginServiceInformation
import org.androidaudioplugin.PluginInformation
import org.androidaudioplugin.PortInformation
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.nio.FloatBuffer


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
    var waveViewSource = state.preview.inBuf
    var waveState by remember { mutableStateOf(waveViewSource) }

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
        WaveformDrawable(waveData = waveState)
        Row {
            Button(onClick = {
                if (!pluginAppliedState) {
                    state.preview.processAudioCompleted = {
                        waveState = state.preview.outBuf
                        pluginAppliedState = true
                    }
                    GlobalScope.launch {
                        state.preview.applyPlugin(state.availablePluginServices.first(), plugin, parameters)
                    }
                } else {
                    waveState = state.preview.inBuf
                    pluginAppliedState = false
                }
            }) { Text(if (pluginAppliedState) "On" else "Off") }
            Button(onClick = {}) { Text("UI") }
            Button(onClick = {
                GlobalScope.launch { state.preview.playSound(pluginAppliedState) }
                }) {
                Text("Play")
            }
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
                                PortInformation.PORT_CONTENT_TYPE_MIDI2 -> "MIDI2"
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

private fun getSampleVisualizationData(floats: FloatBuffer, size: Int, slots: Int) : Sequence<Pair<Float,Float>> {
    return sequence {
        val sizePerGroup = size / slots
        for (y in 0 until slots) {
            var min = Float.MAX_VALUE
            var max = -Float.MIN_VALUE
            for (i in 0 until size / slots / 2) {
                val idx = y * sizePerGroup + i * 2
                val l = floats[idx]
                if (max < l)
                    max = l
                val r = floats[idx + 1]
                if (min > r)
                    min = r
            }
            yield(Pair(max, min))
        }
    }
}

@Composable
fun WaveformDrawable(waveData: ByteArray, height : Dp = 64.dp) {
    val floatBuffer = ByteBuffer.wrap(waveData).order(ByteOrder.LITTLE_ENDIAN).asFloatBuffer()

    Canvas(modifier = Modifier.fillMaxWidth().height(height).border(width = 1.dp, color = Color.Gray), onDraw = {
        val width = this.size.width.toInt()
        val height = this.size.height

        val visualizationData = getSampleVisualizationData(floatBuffer, waveData.size / 4, width).toList()
        var vMin = Float.MAX_VALUE
        var vMax = -Float.MIN_VALUE
        for (pair in visualizationData) {
            val l = pair.first
            if (vMax < l)
                vMax = l
            val r = pair.second
            if (vMin > r)
                vMin = r
        }

        for (wp in visualizationData.indices) {
            var i = wp * 2
            val pair = visualizationData[wp]
            val frL = pair.first / vMax
            val hL = frL * height / 2
            drawLine(
                Color.Black,
                Offset(i.toFloat(), height / 2),
                Offset((i + 1).toFloat(), height / 2 - hL)
            )
            i = wp * 2 + 1
            val frR = pair.second / vMin
            val hR = frR * height / 2
            drawLine(
                Color.Black,
                Offset(i.toFloat(), height / 2),
                Offset((i + 1).toFloat(), height / 2 + hR)
            )
        }
    })
}
