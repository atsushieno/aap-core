package org.androidaudioplugin.ui.compose.app

import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.material3.Checkbox
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import org.androidaudioplugin.PluginInformation
import org.androidaudioplugin.PortInformation
import org.androidaudioplugin.hosting.AudioPluginMidiSettings

@Composable
fun PluginDetails(context: PluginManagerContext, pluginInfo: PluginInformation) {
    val instance = context.instances.firstOrNull { it.pluginInfo.pluginId == pluginInfo.pluginId }
    if (instance == null) {
        if (context.connections.any { it.serviceInfo.packageName == pluginInfo.packageName }) {
            LaunchedEffect(pluginInfo.packageName) {
                context.instantiatePlugin(pluginInfo)
            }
        } else {
            LaunchedEffect(pluginInfo.packageName) {
                context.client.connectToPluginService(pluginInfo.packageName)
            }
        }
    }

    PluginMetadata(pluginInfo)
    if (instance == null)
        Text("Further details are shown after instantiating the plugin...")
    if (instance != null)
        MidiSettings(midiSettingsFlags = instance!!.getMidiMappingPolicy(),
            midiSeetingsFlagsChanged = { newFlags ->
                context.setNewMidiMappingFlags(
                    instance!!,
                    newFlags
                )
            })

    if (instance != null)
        PluginInstanceControl(context, instance)

    val ports =
        if (instance != null)
            (0 until instance.getPortCount()).map { instance.getPort(it) }
        else
            pluginInfo.ports
    PluginPortList(ports)
}


private val headerModifier = Modifier.width(120.dp)

@Composable
private fun ColumnHeader(text: String) {
    Text(modifier = headerModifier, fontWeight = FontWeight.Bold, text = text)
}

@Composable
fun PluginMetadata(pluginInfo: PluginInformation) {
    var pluginDetailsExpanded by remember { mutableStateOf(false) }

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
            Text(pluginInfo.packageName, fontSize = 14.sp)
        }
        Row {
            ColumnHeader("classname: ")
        }
        Row {
            Text(pluginInfo.localName, fontSize = 14.sp)
        }
        if (pluginInfo.developer != null) {
            Row {
                ColumnHeader("developer: ")
            }
            Row {
                Text(pluginInfo.developer ?: "")
            }
        }

        Text(text = "Extensions", fontSize = 20.sp, modifier = Modifier.padding(vertical = 12.dp))
        Column {
            for (extension in pluginInfo.extensions) {
                Row(modifier = Modifier.border(1.dp, Color.LightGray)) {
                    Text(
                        (if (extension.required) "[req]" else "[opt]") + " " + (extension.uri
                            ?: "(uri unspecified)"), fontSize = 12.sp
                    )
                }
            }
        }
    }
}

@Composable
fun MidiSettings(midiSettingsFlags: Int, midiSeetingsFlagsChanged: (Int) -> Unit) {
    var midiSettingsExpanded by remember { mutableStateOf(false) }

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
                        val v = midiSettingsFlags xor AudioPluginMidiSettings.AAP_PARAMETERS_MAPPING_POLICY_PROGRAM
                        midiSeetingsFlagsChanged(v)
                    })
                Text("Consumes Program Changes by its own")
            }
            Row {
                Checkbox(
                    enabled = false,
                    checked = (midiSettingsFlags and AudioPluginMidiSettings.AAP_PARAMETERS_MAPPING_POLICY_CC) != 0,
                    onCheckedChange = {
                        val v = midiSettingsFlags xor AudioPluginMidiSettings.AAP_PARAMETERS_MAPPING_POLICY_CC
                        midiSeetingsFlagsChanged(v)
                    })
                Text("Consumes CCs by its own")
            }
            Row {
                Checkbox(
                    enabled = false,
                    checked = (midiSettingsFlags and AudioPluginMidiSettings.AAP_PARAMETERS_MAPPING_POLICY_ACC) != 0,
                    onCheckedChange = {
                        val v = midiSettingsFlags xor AudioPluginMidiSettings.AAP_PARAMETERS_MAPPING_POLICY_ACC
                        midiSeetingsFlagsChanged(v)
                    })
                Text("Consumes NRPNs (ACCs) by its own")
            }
            Row {
                Checkbox(
                    enabled = false,
                    checked = (midiSettingsFlags and AudioPluginMidiSettings.AAP_PARAMETERS_MAPPING_POLICY_SYSEX8) != 0,
                    onCheckedChange = {
                        val v = midiSettingsFlags xor AudioPluginMidiSettings.AAP_PARAMETERS_MAPPING_POLICY_SYSEX8
                        midiSeetingsFlagsChanged(v)
                    })
                Text("Consumes SysEx8 by its own")
            }
        }
    }
}

@Composable
fun PluginPortList(instancePorts: List<PortInformation>, modifier: Modifier = Modifier) {
    Text(text = "Ports", fontSize = 20.sp, modifier = Modifier.padding(vertical = 12.dp))
    Column {
        for (port in instancePorts) {
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
