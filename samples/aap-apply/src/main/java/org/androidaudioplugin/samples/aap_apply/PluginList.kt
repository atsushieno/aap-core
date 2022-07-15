package org.androidaudioplugin.samples.aap_apply

import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import org.androidaudioplugin.PluginInformation
import org.androidaudioplugin.PluginServiceInformation

@Composable
fun PluginList(onItemClick: (PluginInformation) -> Unit = {}, pluginServices: List<PluginServiceInformation>) {
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
