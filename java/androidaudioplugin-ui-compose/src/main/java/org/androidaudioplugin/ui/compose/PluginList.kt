package org.androidaudioplugin.ui.compose

import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.rememberScrollState
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.TextUnit
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import org.androidaudioplugin.AudioPluginServiceInformation
import org.androidaudioplugin.PluginInformation
import org.androidaudioplugin.PortInformation


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
    rememberScrollState(0f)
    Column {
        Row {
            Text(text = plugin.displayName, fontSize = TextUnit(20))
        }
        Row {
            Header("package: ")
        }
        Row {
            Text(plugin.packageName)
        }
        Row {
            Header("classname: ")
        }
        Row {
            Text(plugin.localName)
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
        Text(text = "Ports", fontSize = TextUnit(20))
        Column {
            for (port in plugin.ports) {
                Row {
                    Header(port.name)
                    Text(
                        text = when (port.content) {
                            PortInformation.PORT_CONTENT_TYPE_AUDIO -> "Audio"
                            PortInformation.PORT_CONTENT_TYPE_MIDI -> "MIDI"
                            else -> "-"
                        }, modifier = Modifier.width(50.dp)
                    )
                    Text(
                        text = when (port.direction) {
                            PortInformation.PORT_DIRECTION_INPUT -> "In"
                            else -> "Out"
                        }, modifier = Modifier.width(30.dp)
                    )
                }
            }
        }
    }
}