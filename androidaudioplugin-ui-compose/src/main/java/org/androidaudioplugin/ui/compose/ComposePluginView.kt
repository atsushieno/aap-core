package org.androidaudioplugin.ui.compose

import android.content.Context
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.requiredHeight
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.itemsIndexed
import androidx.compose.foundation.lazy.rememberLazyListState
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.State
import androidx.compose.runtime.remember
import androidx.compose.runtime.toMutableStateList
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.FontStyle
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import org.androidaudioplugin.AudioPluginServiceHelper
import org.androidaudioplugin.NativeLocalPluginInstance
import org.androidaudioplugin.PluginInformation
import org.androidaudioplugin.PortInformation
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

@Composable
fun ScrollingList() {
    val scrollState = rememberScrollState()
    Column(Modifier.verticalScroll(scrollState)) {
        (0 until 50).forEach {
            Text("Item $it")
        }
    }
}

@Composable
fun PluginViewScope.PluginView(
    modifier: Modifier = Modifier,
    pluginNameLabel: @Composable (String) -> Unit = { Text(it, fontWeight = FontWeight.Bold) },
    getParameterValue: (Int) -> Float,
    onParameterChange: (Int, Float) -> Unit) {

    pluginNameLabel(pluginName)

    Text("FIXME: LazyColumn scroll does not work sufficiently")

    LazyColumn {
        items(parameterCount) { index ->
            val para = getParameter(index)
            Row(Modifier.requiredHeight(60.dp)) {
                Text(
                    para.id.toString(),
                    fontSize = 14.sp,
                    modifier = Modifier.width(60.dp)
                )
                Text(para.name, Modifier.width(120.dp), fontWeight = FontWeight.Bold)
                Text(
                    getParameterValue(index).toString().take(5),
                    fontSize = 14.sp,
                    modifier = Modifier.width(60.dp)
                )
                ImageStripKnob(
                    drawableResId = R.drawable.bright_life,
                    value = getParameterValue(index),
                    valueRange = para.valueRange.start.toFloat()..para.valueRange.endInclusive.toFloat(),
                    onValueChange = { onParameterChange(index, it) }
                )
            }
        }
    }
    LazyColumn {
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
    }
}
