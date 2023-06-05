package org.androidaudioplugin.ui.compose

import android.content.Context
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.requiredHeight
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.grid.GridCells
import androidx.compose.foundation.lazy.grid.LazyVerticalGrid
import androidx.compose.foundation.lazy.itemsIndexed
import androidx.compose.foundation.lazy.rememberLazyListState
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Badge
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.Slider
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.State
import androidx.compose.runtime.remember
import androidx.compose.runtime.toMutableStateList
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.FontStyle
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import org.androidaudioplugin.AudioPluginServiceHelper
import org.androidaudioplugin.NativeLocalPluginInstance
import org.androidaudioplugin.PluginInformation
import org.androidaudioplugin.PortInformation
import org.androidaudioplugin.composeaudiocontrols.DefaultKnobTooltip
import org.androidaudioplugin.composeaudiocontrols.ImageStripKnob

interface PluginViewScope {
    val pluginName: String
    val parameterCount: Int
    fun getParameter(index: Int): PluginViewScopeParameter
    val portCount: Int
    fun getPort(index: Int): PluginViewScopePort
}

interface PluginViewScopePort {
    val name: String
    val direction: Int
    val content: Int
}

interface PluginViewScopeParameter {
    val id: Int
    val name: String
    val value: Double
    val valueRange: ClosedFloatingPointRange<Double>
}

internal class PluginViewScopeImpl(
    val context: Context,
    val instance: NativeLocalPluginInstance,
    val parameters: List<Double>)
    : PluginViewScope {

    private val pluginInfo = AudioPluginServiceHelper.getLocalAudioPluginService(context).plugins.first { it.pluginId == instance.getPluginId() }

    override val pluginName: String
        get() = pluginInfo.displayName

    override val parameterCount: Int
        get() = instance.getParameterCount()

    override fun getParameter(index: Int): PluginViewScopeParameter = PluginViewScopeParameterImpl(this, index, parameters[index])

    override val portCount: Int
        get() = instance.getPortCount()

    override fun getPort(index: Int): PluginViewScopePort = PluginViewScopePortImpl(this, index)
}

internal class PluginViewScopeParameterImpl(private val plugin: PluginViewScopeImpl, index: Int, private val parameterValue: Double) : PluginViewScopeParameter {
    private val info = plugin.instance.getParameter(index)

    override val id: Int
        get() = info.id
    override val name: String
        get() = info.name
    override val value: Double
        get() = parameterValue
    override val valueRange: ClosedFloatingPointRange<Double>
        get() = info.minimumValue.rangeTo(info.maximumValue)
}

internal class PluginViewScopePortImpl(private val plugin: PluginViewScopeImpl, index: Int) : PluginViewScopePort {
    private val info = plugin.instance.getPort(index)

    override val name: String
        get() = info.name
    override val direction: Int
        get() = info.direction
    override val content: Int
        get() = info.content
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun PluginViewScope.PluginView(
    modifier: Modifier = Modifier,
    getParameterValue: (Int) -> Float,
    onParameterChange: (Int, Float) -> Unit) {

    LazyVerticalGrid(columns = GridCells.Fixed(2)) {
        items(parameterCount) { index ->
            val para = getParameter(index)
            Row {
                // Here I use Badge. It is useful when we have to show lengthy parameter name.
                Badge(Modifier.width(100.dp).padding(5.dp, 0.dp)) {
                    Text("${para.id}: ${para.name}")
                }
                ImageStripKnob(
                    drawableResId = R.drawable.bright_life,
                    value = getParameterValue(index),
                    valueRange = para.valueRange.start.toFloat()..para.valueRange.endInclusive.toFloat(),
                    tooltip = { DefaultKnobTooltip(showTooltip = true, value = knobValue )},
                    onValueChange = { onParameterChange(index, it) }
                )
            }
        }
    }
    /* FIXME: we'd like to show them too, but the space is too limited.
    LazyColumn(modifier.height(100.dp)) {
        items(portCount) { index ->
            val port = getPort(index)
            Row {
                Text(port.name, Modifier.width(120.dp), fontWeight = FontWeight.Bold)
                Text(when (port.content) {
                    PortInformation.PORT_CONTENT_TYPE_AUDIO -> "audio"
                    PortInformation.PORT_CONTENT_TYPE_MIDI2 -> "midi2"
                    else -> "other"
                })
                Text(when (port.direction) {
                    PortInformation.PORT_DIRECTION_INPUT -> "in"
                    PortInformation.PORT_DIRECTION_OUTPUT -> "out"
                    else -> "?"
                })
            }
        }
    }*/
}
