package org.androidaudioplugin.ui.compose

import android.annotation.SuppressLint
import android.content.Context
import android.view.View
import android.view.ViewGroup
import android.widget.LinearLayout
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.material3.MaterialTheme
import androidx.compose.runtime.toMutableStateList
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.ComposeView
import org.androidaudioplugin.AudioPluginViewFactory
import org.androidaudioplugin.NativePluginService
import org.androidaudioplugin.hosting.UmpHelper
import java.nio.ByteBuffer
import java.nio.ByteOrder

class ComposeAudioPluginViewFactory : AudioPluginViewFactory() {
    override fun createView(context: Context, pluginId: String, instanceId: Int) = ComposeAudioPluginView(context, pluginId, instanceId) as View
}

@SuppressLint("ViewConstructor")
internal class ComposeAudioPluginView(context: Context, pluginId: String, instanceId: Int) : LinearLayout(context) {
    private val composeView = ComposeView(context)
    private val instance = NativePluginService(pluginId).getInstance(instanceId)
    private val parameters = (0 until instance.getParameterCount())
        .map { instance.getParameter(it).defaultValue }
        .toMutableStateList()
    private val scope = PluginViewScopeImpl(context, instance, parameters.toList())
    private val paramChangeBuffer = ByteBuffer.allocateDirect(32).order(ByteOrder.nativeOrder()) // 16 should be fine as of V3 protocol

    init {
        layoutParams = ViewGroup.LayoutParams(width, height)
        addView(composeView)

        val onMidi2Note = { status: Int, note: Int, _: Long ->
            paramChangeBuffer.clear()
            paramChangeBuffer.asIntBuffer().put((0x40 shl 24) + (status shl 16) + (note shl 8)).put(0xf8 shl 24)
            instance.addEventUmpInput(paramChangeBuffer, 8)
        }

        composeView.setContent {
            MaterialTheme {
                Column(Modifier.background(MaterialTheme.colorScheme.background)
                    .fillMaxSize(1f)) {
                    scope.PluginView(
                        getParameterValue = { index -> parameters[index].toFloat() },
                        onParameterChange = { index, value ->
                            val ump = UmpHelper.aapUmpSysex8Parameter(instance.getParameter(index).id.toUInt(), value)
                            paramChangeBuffer.clear()
                            paramChangeBuffer.asIntBuffer().put(ump.toIntArray())
                            instance.addEventUmpInput(paramChangeBuffer, ump.size * 4)
                            parameters[index] = value.toDouble()
                        },
                        onNoteOn = { note, details -> onMidi2Note(0x90, note, details) },
                        onNoteOff = { note, details -> onMidi2Note(0x80, note, details) }
                    )
                }
            }
        }
    }
}
