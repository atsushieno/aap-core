package org.androidaudioplugin.ui.compose

import android.content.Context
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Box
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
import androidx.compose.material3.Checkbox
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Slider
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.State
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
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
import org.androidaudioplugin.ParameterInformation
import org.androidaudioplugin.PluginInformation
import org.androidaudioplugin.PortInformation
import org.androidaudioplugin.composeaudiocontrols.DefaultKnobTooltip
import org.androidaudioplugin.composeaudiocontrols.DiatonicKeyboard
import org.androidaudioplugin.composeaudiocontrols.DiatonicKeyboardMoveAction
import org.androidaudioplugin.composeaudiocontrols.DiatonicKeyboardNoteExpressionOrigin
import org.androidaudioplugin.composeaudiocontrols.DiatonicKeyboardWithControllers
import org.androidaudioplugin.composeaudiocontrols.ImageStripKnob
import kotlin.math.abs

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
    fun getEnumerationCount(): Int
    fun getEnumeration(index: Int): PluginViewScopeEnumeration
}

interface PluginViewScopeEnumeration {
    val index: Int
    val value: Double
    val name: String
}

// I wanted to have those classes internal, but they have to be referenced from ui-compose-app too...

class PluginViewScopeImpl(
    val context: Context,
    val instance: NativeLocalPluginInstance,
    val parameters: List<Double>)
    : PluginViewScope {

    private val pluginInfo = AudioPluginServiceHelper.getLocalAudioPluginService(context).plugins.first { it.pluginId == instance.getPluginId() }

    override val pluginName: String
        get() = pluginInfo.displayName

    override val parameterCount: Int
        get() = instance.getParameterCount()

    override fun getParameter(index: Int): PluginViewScopeParameter = PluginViewScopeParameterImpl(instance.getParameter(index), parameters[index])

    override val portCount: Int
        get() = instance.getPortCount()

    override fun getPort(index: Int): PluginViewScopePort = PluginViewScopePortImpl(instance.getPort(index))
}

class PluginViewScopeParameterImpl(private val info: ParameterInformation, private val parameterValue: Double) : PluginViewScopeParameter {
    override val id: Int
        get() = info.id
    override val name: String
        get() = info.name
    override val value: Double
        get() = parameterValue
    override val valueRange: ClosedFloatingPointRange<Double>
        get() = info.minimumValue.rangeTo(info.maximumValue)

    override fun getEnumerationCount() = info.enumerations.size

    override fun getEnumeration(index: Int): PluginViewScopeEnumeration {
        val e = info.enumerations[index]
        return PluginViewScopeEnumerationImpl(e.index, e.value, e.name)
    }
}

data class PluginViewScopeEnumerationImpl(
    override val index: Int,
    override val value: Double,
    override val name: String)
    : PluginViewScopeEnumeration

class PluginViewScopePortImpl(private val info: PortInformation) : PluginViewScopePort {
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
    onParameterChange: (Int, Float) -> Unit,
    onNoteOn: (Int, Long) -> Unit = { _,_ -> },
    onNoteOff: (Int, Long) -> Unit = { _,_ -> },
    onExpression: (DiatonicKeyboardNoteExpressionOrigin, Int, Float) -> Unit = { _,_,_ -> }) {

    // Keyboard controllers
    // This looks hacky, but it is done for compact yet touchable UI parts.
    val noteOnStates = remember { List(128) { 0L }.toMutableStateList() }
    var exprMode by remember { mutableStateOf(false) }
    var octave by remember { mutableStateOf(4) }
    Row {
        Row(Modifier.clickable { exprMode = !exprMode }) {
            Text("[", fontSize = 18.sp)
            Text(if (exprMode) "\u2714" else "", Modifier.width(20.dp), fontSize = 18.sp)
            Text("] PerNote Expr.", fontSize = 18.sp)
        }
        Text(" ", Modifier.width(60.dp))
        Text("[\u25C0]", fontSize = 18.sp, modifier = Modifier.clickable { if (octave > 0) octave-- })
        Text("Octave: $octave", fontSize = 18.sp)
        Text("[\u25B6]", fontSize = 18.sp, modifier = Modifier.clickable { if (octave < 9) octave++ })
    }

    DiatonicKeyboard(noteOnStates.toList(),
        modifier = modifier,
        octaveZeroBased = octave,
        moveAction = if (exprMode) DiatonicKeyboardMoveAction.NoteExpression else DiatonicKeyboardMoveAction.NoteChange,
        // you will also insert actual musical operations within these lambdas
        onNoteOn = { note, _ ->
            noteOnStates[note] = 1
            onNoteOn(note, 0)
        },
        onNoteOff = { note, _ ->
            noteOnStates[note] = 0
            onNoteOff(note, 0)
        },
        onExpression = { origin, note, data ->
            onExpression(origin, note, data)
        }
    )

    // parameter list
    LazyVerticalGrid(columns = GridCells.Fixed(2), modifier = Modifier.height(200.dp)) {
        items(parameterCount) { index ->
            val para = getParameter(index)
            Row {
                // Here I use Badge. It is useful when we have to show lengthy parameter name.
                Badge(
                    Modifier
                        .width(100.dp)
                        .padding(5.dp, 0.dp)) {
                    Text("${para.id}: ${para.name}")
                }
                if (para.getEnumerationCount() > 0) {
                    val enums = (0 until para.getEnumerationCount()).map { para.getEnumeration(it) }.sortedBy { it.value }
                    val minValue = enums.minBy { it.value }.value
                    val maxValue = enums.maxBy { it.value }.value
                    // a dirty hack to show the matching enum value from current value.
                    // (cannot be "the nearest enum" as we round it.)
                    // FIXME: we would need semantic definition for matching.
                    val getMatchedEnum = { f:Float -> enums.last { it.value <= f } }
                    val v = getParameterValue(index)
                    ImageStripKnob(
                        drawableResId = R.drawable.bright_life,
                        value = v,
                        valueRange = minValue.toFloat() .. maxValue.toFloat(),
                        tooltip = {
                            val en = getMatchedEnum(v)
                            Text(if (en.name.length > 9) en.name.take(8) + ".." else en.name,
                                fontSize = 12.sp,
                                color = Color.Gray)
                        },
                        onValueChange = { onParameterChange(index, it) }
                    )
                } else {
                    ImageStripKnob(
                        drawableResId = R.drawable.bright_life,
                        value = getParameterValue(index),
                        valueRange = para.valueRange.start.toFloat()..para.valueRange.endInclusive.toFloat(),
                        tooltip = { DefaultKnobTooltip(showTooltip = true, value = knobValue) },
                        onValueChange = { onParameterChange(index, it) }
                    )
                }
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
