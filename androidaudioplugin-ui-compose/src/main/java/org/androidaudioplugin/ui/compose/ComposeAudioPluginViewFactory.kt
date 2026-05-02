package org.androidaudioplugin.ui.compose

import android.annotation.SuppressLint
import android.content.Context
import android.os.Build
import android.os.Handler
import android.os.Looper
import android.view.View
import android.view.ViewGroup
import android.widget.LinearLayout
import androidx.annotation.RequiresApi
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.material3.MaterialTheme
import androidx.compose.runtime.toMutableStateMap
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.ComposeView
import org.androidaudioplugin.AudioPluginViewFactory
import org.androidaudioplugin.NativePluginService
import org.androidaudioplugin.hosting.UmpHelper
import java.nio.ByteBuffer
import java.nio.ByteOrder
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.cancel
import kotlinx.coroutines.delay
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch

class ComposeAudioPluginViewFactory : AudioPluginViewFactory() {
    @RequiresApi(Build.VERSION_CODES.R)
    override fun createView(context: Context, pluginId: String, instanceId: Int) = ComposeAudioPluginView(context, pluginId, instanceId) as View
}

@RequiresApi(Build.VERSION_CODES.R)
@SuppressLint("ViewConstructor", "UnusedMaterial3ScaffoldPaddingParameter")
internal class ComposeAudioPluginView(context: Context, pluginId: String, instanceId: Int) : LinearLayout(context) {
    private val composeView = ComposeView(context)
    private val pluginId = pluginId
    private val instanceId = instanceId
    private val instance = NativePluginService(pluginId).getInstance(instanceId)
    private val parameters = (0 until instance.getParameterCount())
        .map { it to instance.getParameter(it).defaultValue }
        .toMutableStateMap()
    private val parameterIndexById = (0 until instance.getParameterCount())
        .associate { index -> instance.getParameter(index).id to index }
    private val parameterInfoById = (0 until instance.getParameterCount())
        .associate { index -> instance.getParameter(index).id to instance.getParameter(index) }
    private val scope = PluginViewScopeImpl(context, instance, parameters)
    private val paramChangeBuffer = ByteBuffer.allocateDirect(32).order(ByteOrder.nativeOrder()) // 16 should be fine as of V3 protocol
    private val parameterOutputBuffer = ByteBuffer.allocateDirect(8192).order(ByteOrder.nativeOrder())
    private val parameterListenerScope = CoroutineScope(SupervisorJob() + Dispatchers.Default)
    private val mainHandler = Handler(Looper.getMainLooper())
    private var parameterListenerJob: Job? = null

    init {
        layoutParams = ViewGroup.LayoutParams(width, height)
        addView(composeView)
        startParameterListener()

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
                        getParameterValue = { parameterIndex -> (parameters.toMap()[parameterIndex] ?: instance.getParameter(parameterIndex).defaultValue).toFloat() },
                        onParameterChange = { parameterIndex, value ->
                            val parameter = instance.getParameter(parameterIndex)
                            val ump = UmpHelper.aapUmpSysex8ParameterPlain(parameter.id.toUInt(), parameter.minimumValue, parameter.maximumValue, value.toDouble())
                            paramChangeBuffer.clear()
                            paramChangeBuffer.asIntBuffer().put(ump.toIntArray())
                            instance.addEventUmpInput(paramChangeBuffer, ump.size * 4)
                            parameters[parameterIndex] = value.toDouble()
                        },
                        onPresetChange = { index -> instance.setPresetIndex(index) },
                        onNoteOn = { note, details -> onMidi2Note(0x90, note, details) },
                        onNoteOff = { note, details -> onMidi2Note(0x80, note, details) }
                    )
                }
            }
        }
    }

    override fun onDetachedFromWindow() {
        parameterListenerJob?.cancel()
        parameterListenerScope.cancel()
        super.onDetachedFromWindow()
    }

    private fun startParameterListener() {
        if (parameterListenerJob != null)
            return
        parameterListenerJob = parameterListenerScope.launch {
            while (isActive) {
                var sawUpdate = false
                while (isActive) {
                    parameterOutputBuffer.clear()
                    val bytesRead = readParameterOutput(pluginId, instanceId, parameterOutputBuffer, parameterOutputBuffer.capacity())
                    if (bytesRead <= 0)
                        break
                    val updates = parseParameterUpdates(parameterOutputBuffer, bytesRead)
                    if (updates.isNotEmpty()) {
                        sawUpdate = true
                        mainHandler.post {
                            updates.forEach { update ->
                                parameterIndexById[update.parameterId]?.let { index ->
                                    parameters[index] = update.value
                                }
                            }
                        }
                    }
                }
                delay(if (sawUpdate) 1 else 16)
            }
        }
    }

    private fun parseParameterUpdates(buffer: ByteBuffer, bytesRead: Int): List<ParameterUpdate> {
        val updates = mutableListOf<ParameterUpdate>()
        buffer.position(0)
        buffer.limit(bytesRead)
        while (buffer.remaining() >= Int.SIZE_BYTES) {
            val offset = buffer.position()
            val word0 = buffer.int
            when ((word0 ushr 28) and 0xF) {
                4 -> {
                    if (buffer.remaining() < Int.SIZE_BYTES) {
                        buffer.position(bytesRead)
                        continue
                    }
                    val word1 = buffer.int
                    val status = (word0 ushr 16) and 0xF0
                    val channel = (word0 ushr 16) and 0xF
                    if (channel != 0)
                        continue
                    when (status) {
                        0x30 -> {
                            val parameterId = ((word0 ushr 8) and 0x7F) shl 7 or (word0 and 0x7F)
                            val parameter = parameterInfoById[parameterId] ?: continue
                            updates += ParameterUpdate(parameterId, UmpHelper.transportUint32ToPlain(parameter.minimumValue, parameter.maximumValue, word1))
                        }
                        0xB0 -> {
                            val parameterId = (word0 ushr 8) and 0x7F
                            val parameter = parameterInfoById[parameterId] ?: continue
                            updates += ParameterUpdate(parameterId, UmpHelper.transportUint32ToPlain(parameter.minimumValue, parameter.maximumValue, word1))
                        }
                    }
                }
                5 -> {
                    if (buffer.remaining() < Int.SIZE_BYTES * 3) {
                        buffer.position(bytesRead)
                        continue
                    }
                    val word1 = buffer.int
                    val word2 = buffer.int
                    val word3 = buffer.int
                    val channel = word1 and 0xF
                    if (channel == 0 && (word0 and 0xFF) == 0x7E && word1 ushr 8 == 0x7F0000) {
                        val parameterId = word2 and 0xFFFF
                        val parameter = parameterInfoById[parameterId] ?: continue
                        updates += ParameterUpdate(parameterId, UmpHelper.transportUint32ToPlain(parameter.minimumValue, parameter.maximumValue, word3))
                    }
                }
                else -> {
                    val messageSize = when ((word0 ushr 28) and 0xF) {
                        0, 1, 2, 6, 7 -> Int.SIZE_BYTES
                        3, 4, 8, 9, 0xA -> Int.SIZE_BYTES * 2
                        0xB, 0xC -> Int.SIZE_BYTES * 3
                        else -> Int.SIZE_BYTES * 4
                    }
                    buffer.position((offset + messageSize).coerceAtMost(bytesRead))
                }
            }
        }
        return updates
    }

    private data class ParameterUpdate(val parameterId: Int, val value: Double)

    private external fun readParameterOutput(pluginId: String, instanceId: Int, buffer: ByteBuffer, size: Int): Int
}
