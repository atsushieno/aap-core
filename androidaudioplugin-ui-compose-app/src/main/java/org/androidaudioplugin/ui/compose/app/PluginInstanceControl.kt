package org.androidaudioplugin.ui.compose.app

import android.content.Context
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.toMutableStateList
import org.androidaudioplugin.PluginInformation
import org.androidaudioplugin.hosting.NativeRemotePluginInstance
import org.androidaudioplugin.ui.compose.PluginView
import org.androidaudioplugin.ui.compose.PluginViewScope
import org.androidaudioplugin.ui.compose.PluginViewScopeParameter
import org.androidaudioplugin.ui.compose.PluginViewScopeParameterImpl
import org.androidaudioplugin.ui.compose.PluginViewScopePort
import org.androidaudioplugin.ui.compose.PluginViewScopePortImpl

internal class RemotePluginViewScopeImpl(
    val context: Context,
    val instance: NativeRemotePluginInstance,
    val parameters: List<Double>,
    val pluginInfo: PluginInformation)
    : PluginViewScope {

    override val pluginName: String
        get() = pluginInfo.displayName

    override val parameterCount: Int
        get() = instance.getParameterCount()

    override fun getParameter(index: Int): PluginViewScopeParameter = PluginViewScopeParameterImpl(instance.getParameter(index), parameters[index])

    override val portCount: Int
        get() = instance.getPortCount()

    override fun getPort(index: Int): PluginViewScopePort = PluginViewScopePortImpl(instance.getPort(index))
}

@Composable
fun PluginInstanceControl(context: PluginManagerContext, pluginInfo: PluginInformation, instance: NativeRemotePluginInstance) {

    TextButton(onClick = { context.playPreloadedAudio() }) {
        Text(text = "Play Audio")
    }

    // FIXME: should this be hoisted out? It feels like performance loss
    val parameters = remember { (0 until instance.getParameterCount())
        .map { instance.getParameter(it).defaultValue }
        .toMutableStateList() }
    val scope by remember { mutableStateOf(RemotePluginViewScopeImpl(context.context, instance, parameters, pluginInfo)) }
    scope.PluginView(getParameterValue = { parameters[it].toFloat() },
        onParameterChange = { index, value ->
            parameters[index] = value.toDouble()
            context.setParameterValue(index.toUInt(), value)
                            },
        onNoteOn = { note, _ -> context.setNoteState(note, true) },
        onNoteOff = { note, _ ->  context.setNoteState(note, false) },
        onExpression = {origin, note, value -> context.processExpression(origin, note, value) })
}