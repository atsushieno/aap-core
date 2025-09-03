package org.androidaudioplugin.ui.compose.app

import android.os.Build
import androidx.annotation.RequiresApi
import androidx.compose.foundation.Image
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.LazyListState
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.Text
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.ExposedDropdownMenuBox
import androidx.compose.material3.ExposedDropdownMenuDefaults
import androidx.compose.material3.TextField
import androidx.compose.runtime.Composable
import androidx.compose.runtime.SideEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.google.accompanist.drawablepainter.rememberDrawablePainter
import com.google.accompanist.permissions.ExperimentalPermissionsApi
import com.google.accompanist.permissions.PermissionStatus
import com.google.accompanist.permissions.rememberPermissionState
import kotlinx.coroutines.coroutineScope
import kotlinx.coroutines.launch

@OptIn(ExperimentalMaterial3Api::class, ExperimentalPermissionsApi::class)
@Composable
fun PluginList(context: PluginManagerScope,
               onSelectItem: (pluginId: String) -> Unit) {

    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
        val permissionState = rememberPermissionState(android.Manifest.permission.POST_NOTIFICATIONS)
        SideEffect {
            if (permissionState.status != PermissionStatus.Granted) {
                permissionState.launchPermissionRequest()
            }
        }
    }

    val pluginServices = remember { context.pluginServices }

    val deselectedLabel = "-- jump to plugin service --"
    // Plugin Service selector
    var selectedPackage by remember { mutableStateOf("") }
    // the selected package might disappear from the provided list. Empty it if that happens.
    if (!pluginServices.any { it.packageName == selectedPackage })
        selectedPackage = ""

    val listState = remember { LazyListState() }
    val plugins = remember { pluginServices.flatMap { it.plugins } }
    val coroutineScope = rememberCoroutineScope()

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
                }, text = {
                    Text(deselectedLabel)
                })
                pluginServices.forEach {
                    DropdownMenuItem(onClick = {
                        val matchingPluginIndex = plugins.indexOfFirst { p -> p.packageName == it.packageName }
                        coroutineScope.launch {
                            listState.animateScrollToItem(matchingPluginIndex)
                        }
                        if (expanded)
                            selectedPackage = it.packageName
                        expanded = !expanded
                    }, text = {
                        Text(it.label)
                    })
                }
            }
        }
    }

    LazyColumn(state = listState) {
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