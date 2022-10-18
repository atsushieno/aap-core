package org.androidaudioplugin.ui.compose2

import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import org.androidaudioplugin.PluginInformation

@Composable
fun PluginList(state: PluginListAppState, onItemClick: (PluginInformation) -> Unit = {}) {
    val small = TextStyle(fontSize = 12.sp)

    LazyColumn {
        items(state.engine.availablePluginServices.flatMap { s -> s.plugins }) { plugin ->
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

