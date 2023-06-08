package org.androidaudioplugin.ui.compose

import androidx.compose.foundation.layout.width
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
import androidx.compose.ui.unit.dp
import org.androidaudioplugin.ParameterInformation

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun PluginParameterEnumSelector(value: Float,
                                enumerations: List<ParameterInformation.EnumerationInformation>,
                                onValueChange: (value: Double) -> Unit) {
    val v = value.toDouble()
    var enumExpanded by remember { mutableStateOf(false) }
    ExposedDropdownMenuBox(
        expanded = enumExpanded, onExpandedChange = { enumExpanded = it }) {
        val name = (enumerations.firstOrNull { it.value == v } ?: enumerations.first()).name
        TextField(
            name, onValueChange = {}, readOnly = true,
            trailingIcon = { ExposedDropdownMenuDefaults.TrailingIcon(expanded = enumExpanded) },
            modifier = Modifier.menuAnchor()
        )
        ExposedDropdownMenu(
            expanded = enumExpanded,
            onDismissRequest = { enumExpanded = false }) {
            enumerations.forEach {
                DropdownMenuItem(onClick = {
                    if (enumExpanded)
                        onValueChange(it.value)
                    enumExpanded = !enumExpanded
                }) {
                    Text(it.name)
                }
            }
        }
    }
}