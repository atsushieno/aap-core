package org.androidaudioplugin.midideviceservice

import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.LazyListState
import androidx.compose.foundation.lazy.itemsIndexed
import androidx.compose.material.Button
import androidx.compose.material.Checkbox
import androidx.compose.material.MaterialTheme
import androidx.compose.material.Surface
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import org.androidaudioplugin.PluginInformation
import org.androidaudioplugin.midideviceservice.ui.theme.AAPMidiDeviceServiceTheme

interface WebUIUpdater {
    fun setInstrumentPlugin(plugin: PluginInformation)
    fun setMidi2Enabled(value: Boolean)
    fun initializeMidi()
    fun terminateMidi()
    fun playNote()
}

private object updater: WebUIUpdater {
    override fun setInstrumentPlugin(plugin: PluginInformation) {
        model.specifiedInstrument = plugin
    }

    override fun setMidi2Enabled(value: Boolean) {
        model.useMidi2Protocol = value
    }

    override fun initializeMidi() {
        model.initializeMidi()
    }

    override fun terminateMidi() {
        model.terminateMidi()
    }

    override fun playNote() {
        model.playNote()
    }
}

@Composable
fun App() {
    val plugins: List<PluginInformation> = model.pluginServices.flatMap { s -> s.plugins }.toList()
        .filter { p -> p.category?.contains("Instrument") ?: false || p.category?.contains("Synth") ?: false }

    AAPMidiDeviceServiceTheme {
        Surface(color = MaterialTheme.colors.background) {
            Column {
                AvailablePlugins(onItemClick = { plugin -> updater.setInstrumentPlugin(plugin) }, plugins)
                PluginInstanceControllerUI()
            }
        }
    }
}

@Composable
fun AvailablePlugins(onItemClick: (PluginInformation) -> Unit = {}, instrumentPlugnis: List<PluginInformation>) {
    val small = TextStyle(fontSize = 12.sp)

    val state by remember { mutableStateOf(LazyListState()) }
    var selectedIndex by remember { mutableStateOf(if (model.specifiedInstrument != null) instrumentPlugnis.indexOf(model.specifiedInstrument!!) else -1) }

    LazyColumn(state = state) {
        itemsIndexed(instrumentPlugnis, itemContent = { index, plugin ->
            Row(modifier = Modifier
                .padding(start = 16.dp, end = 16.dp)
            ) {
                Column(modifier = Modifier
                    .clickable {
                        onItemClick(plugin)
                        selectedIndex = index
                    }
                    .border(
                        if (index == selectedIndex) 2.dp else 0.dp,
                        MaterialTheme.colors.primary
                    )
                    .weight(1f)) {
                    Text(plugin.displayName)
                    Text(plugin.packageName, style = small)
                }
            }
        })
    }
}

@Composable
fun PluginInstanceControllerUI() {
    // FIXME: maybe we should remove this hacky state variable
    var midiManagerInitializedState by remember { mutableStateOf(model.midiManagerInitialized) }
    var useMidi2ProtocolState by remember { mutableStateOf(model.useMidi2Protocol) }

    Row {
        if (midiManagerInitializedState)
            Button(modifier = Modifier.padding(2.dp),
                onClick = {
                    updater.terminateMidi()
                    midiManagerInitializedState = false
                }) {
                Text("Stop MIDI Service")
            }
        else
            Button(modifier = Modifier.padding(2.dp),
                onClick = {
                    updater.initializeMidi()
                    midiManagerInitializedState = true
                }) {
                Text("Start MIDI Service")
            }
    }
    Row {
        Checkbox(checked = useMidi2ProtocolState, onCheckedChange = { value ->
            updater.setMidi2Enabled(value)
            useMidi2ProtocolState = value
        })
        Text("Use MIDI 2.0 Protocol")
        Button(modifier = Modifier.padding(2.dp),
            onClick = { updater.playNote() }) {
            Text("Play")
        }

    }
}
