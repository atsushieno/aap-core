package org.androidaudioplugin.ui.compose.app

import android.app.job.JobScheduler
import android.os.Build
import android.view.View
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.material3.Button
import androidx.compose.material3.Checkbox
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.SideEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCompositionContext
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.runtime.withRunningRecomposer
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import kotlinx.coroutines.GlobalScope
import kotlinx.coroutines.launch
import kotlinx.coroutines.runBlocking
import kotlinx.coroutines.runInterruptible
import org.androidaudioplugin.PluginInformation
import org.androidaudioplugin.PortInformation
import org.androidaudioplugin.hosting.AudioPluginHostHelper
import org.androidaudioplugin.hosting.AudioPluginMidiSettings
import org.androidaudioplugin.hosting.AudioPluginSurfaceControlClient
import kotlin.coroutines.coroutineContext

@Composable
fun PluginDetails(pluginInfo: PluginInformation, manager: PluginManagerScope) {
    // PluginDetailsScope.create() involves suspend fun, so we replace the Composable once it's ready.
    var scope by remember { mutableStateOf<PluginDetailsScope?>(null) }

    LaunchedEffect(key1 = pluginInfo) {
        scope = PluginDetailsScope.create(pluginInfo, manager)
    }

    val currentScope = scope
    if (currentScope == null) {
        LaunchedEffect(pluginInfo.packageName) {
            if (!manager.connections.any { it.serviceInfo.packageName == pluginInfo.packageName })
                manager.client.connectToPluginService(pluginInfo.packageName)
        }
        PluginDetailsInstancing(pluginInfo)
    }
    else
        PluginDetailsInstantiated(currentScope)
}

@Composable
fun PluginDetailsInstancing(pluginInfo: PluginInformation) {
    Text("Instantiating ${pluginInfo.displayName} ...")
    PluginMetadata(pluginInfo)
    PluginPortList(pluginInfo.ports)
}

@Composable
fun PluginDetailsInstantiated(scope: PluginDetailsScope) {
    val pluginInfo = scope.pluginInfo

    var showWebUI by remember { mutableStateOf(false) }
    var showSurfaceUI by remember { mutableStateOf(false) }
    var surfaceUIConnected by remember { mutableStateOf(false) }
    val instance = scope.instance!!
    var surfaceUIScope by remember { mutableStateOf<SurfaceControlUIScope?>(null) }

    Box {
        // We show the Web UI and the native UI *outside* any of the sequential layout contexts e.g.
        // Column() or Row(), so we have WebUI and SurfaceControl UI on top level along with the main content.
        //
        // Their visibility state (showXxxUI) affects the content of PluginInstanceControl() so they
        // have to be passed to the composable, along with onXxxChanged handlers...
        Column {
            PluginMetadata(pluginInfo)
            val ports = (0 until instance.getPortCount()).map { instance.getPort(it) }
            PluginPortList(ports)

            PluginInstanceControl(
                scope, pluginInfo, instance
            )

            Row {
                Button(onClick = {
                    showWebUI = !showWebUI
                }) {
                    Text(if (showWebUI) "Hide Web UI" else "Show Web UI")
                }

                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
                    Button(onClick = {
                        showSurfaceUI = !showSurfaceUI
                    }) {
                        Text(if (showSurfaceUI) "Hide Native UI" else "Show Native UI")
                    }
                }
            }
        }

        if (showWebUI)
            PluginWebUI(pluginInfo.packageName, pluginInfo.pluginId!!, instance,
                onCloseClick = { showWebUI = false })

        if (showSurfaceUI) {
            val uiScope = surfaceUIScope
            if (uiScope == null) {
                LaunchedEffect(scope) {
                    // FIXME: pass appropriate sizes here
                    surfaceUIScope = SurfaceControlUIScope.create(scope, 1000, 750)
                }
            } else {
                // We can call this composable only after we create the SurfaceControl client.
                PluginSurfaceControlUI(
                    pluginInfo,
                    createSurfaceView = { _ -> uiScope.surfaceControl!!.surfaceView },
                    detachSurfaceView = { _ -> surfaceUIConnected = false },
                    onCloseClick = { showSurfaceUI = false })

                if (!surfaceUIConnected) {
                    LaunchedEffect(uiScope) {
                        // Connection has to be established only after AndroidView is added to the view tree
                        // and displayId becomes available.
                        uiScope.connectSurfaceControlUI()
                        surfaceUIConnected = true
                    }
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
    var portListExpanded by remember { mutableStateOf(false) }

    Text(text = (if (portListExpanded) "[-]" else "[+]") + " Ports", fontSize = 20.sp, modifier = Modifier
        .padding(vertical = 12.dp)
        .clickable {
            portListExpanded = !portListExpanded
        })

    if (!portListExpanded)
        return

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
