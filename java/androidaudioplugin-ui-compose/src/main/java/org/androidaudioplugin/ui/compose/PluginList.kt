package org.androidaudioplugin.ui.compose

import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.rememberScrollState
import androidx.compose.material.Slider
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.getValue
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import org.androidaudioplugin.AudioPluginServiceInformation
import org.androidaudioplugin.PluginInformation
import org.androidaudioplugin.PortInformation
import kotlin.Float.Companion


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

@Composable
fun PluginDetails(plugin: PluginInformation) {
    rememberScrollState(0)
    Column(modifier = Modifier.padding(8.dp)) {
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
                        onValueChange = { sliderPosition = it })
                }
            }
        }
    }
}