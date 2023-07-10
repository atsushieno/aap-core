package org.androidaudioplugin.ui.compose.app

import androidx.compose.foundation.Image
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.material.DropdownMenuItem
import androidx.compose.material.Text
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.ExposedDropdownMenuBox
import androidx.compose.material3.ExposedDropdownMenuDefaults
import androidx.compose.material3.TextField
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.google.accompanist.drawablepainter.rememberDrawablePainter

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun PluginList(context: PluginManagerContext,
               onSelectItem: (pluginId: String) -> Unit) {
    val pluginServices = remember { context.pluginServices }

    val deselectedLabel = "-- all plugin services --"
    // Plugin Service selector
    var selectedPackage by remember { mutableStateOf("") }
    // the selected package might disappear from the provided list. Empty it if that happens.
    if (!pluginServices.any { it.packageName == selectedPackage })
        selectedPackage = ""

    if (pluginServices.size > 1) {
        var expanded by remember { mutableStateOf(false) }
        ExposedDropdownMenuBox(
            expanded = expanded, onExpandedChange = { expanded = it }) {
            val name =
                if (selectedPackage == "") deselectedLabel else pluginServices.first { it.packageName == selectedPackage }.label
            TextField(
                name, onValueChange = {}, readOnly = true,
                trailingIcon = { ExposedDropdownMenuDefaults.TrailingIcon(expanded = expanded) },
                modifier = Modifier.menuAnchor()
            )
            ExposedDropdownMenu(
                expanded = expanded,
                onDismissRequest = { expanded = false }) {
                DropdownMenuItem(onClick = {
                    selectedPackage = ""
                    expanded = !expanded
                }) {
                    Text(deselectedLabel)
                }
                pluginServices.forEach {
                    DropdownMenuItem(onClick = {
                        if (expanded)
                            selectedPackage = it.packageName
                        expanded = !expanded
                    }) {
                        Text(it.label)
                    }
                }
            }
        }
    }

    LazyColumn {
        val plugins = pluginServices.flatMap { it.plugins }
            .filter { selectedPackage == "" || it.packageName == selectedPackage }
        items(plugins.size) { index ->
            val plugin = plugins[index]
            Row(
                modifier = Modifier
                    .padding(start = 16.dp, end = 16.dp)
                    .then(Modifier.clickable { onSelectItem(plugin.pluginId!!) })
            ) {
                val small = TextStyle(fontSize = 12.sp)
                val icon = context.context.packageManager.getApplicationIcon(plugin.packageName)
                Image(
                    painter = rememberDrawablePainter(icon),
                    contentDescription = "Icon for ${plugin.packageName}",
                    contentScale = ContentScale.Fit,
                    modifier = Modifier
                        .size(32.dp)
                        .padding(4.dp)
                )
                Column(modifier = Modifier.weight(1f)) {
                    Text(plugin.displayName)
                    Text(plugin.pluginId ?: "", style = small)
                }
            }
        }
    }
}