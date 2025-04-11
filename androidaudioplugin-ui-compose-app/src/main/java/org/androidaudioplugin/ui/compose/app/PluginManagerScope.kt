package org.androidaudioplugin.ui.compose.app

import android.content.Context
import android.media.AudioManager
import android.os.Build
import android.view.View
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.snapshots.SnapshotStateList
import androidx.compose.runtime.toMutableStateList
import org.androidaudioplugin.PluginInformation
import org.androidaudioplugin.PluginServiceInformation
import org.androidaudioplugin.composeaudiocontrols.DiatonicKeyboardNoteExpressionOrigin
import org.androidaudioplugin.hosting.AudioPluginClientBase
import org.androidaudioplugin.hosting.AudioPluginHostHelper
import org.androidaudioplugin.hosting.AudioPluginMidiSettings
import org.androidaudioplugin.hosting.AudioPluginSurfaceControlClient
import org.androidaudioplugin.hosting.NativeRemotePluginInstance
import org.androidaudioplugin.hosting.PluginServiceConnection
import org.androidaudioplugin.manager.PluginPlayer

class PluginManagerScope(val context: Context,
                         val pluginServices: SnapshotStateList<PluginServiceInformation>
) : AutoCloseable {
    val logTag = "AAPPluginManager"

    val client = AudioPluginClientBase(context).apply {
        onConnectedListeners.add { conn -> connections.add(conn) }
        onDisconnectingListeners.add { conn -> connections.remove(conn) }
    }

    // An observable list version of service connections
    val connections = listOf<PluginServiceConnection>().toMutableStateList()

    override fun close() {
        client.dispose()
    }
}

class PluginDetailsScope(val pluginInfo: PluginInformation,
                         val manager: PluginManagerScope) : AutoCloseable {
    var instance = mutableStateOf<NativeRemotePluginInstance?>(null)

    private val pluginPlayer by lazy {
        val audioManager = manager.context.getSystemService(Context.AUDIO_SERVICE) as AudioManager
        val sampleRate = audioManager.getProperty(AudioManager.PROPERTY_OUTPUT_SAMPLE_RATE).toInt()
        // It is for the audio processor's callback
        // FIXME: make them configurable?
        val frames = audioManager.getProperty(AudioManager.PROPERTY_OUTPUT_FRAMES_PER_BUFFER).toInt()
        val channelCount = 2
        PluginPlayer.create(sampleRate, frames, channelCount).apply {
            setPlugin(instance.value!!)
            manager.context.assets.open(PluginPlayer.sample_audio_filename).use {
                val bytes = ByteArray(it.available())
                it.read(bytes)
                loadAudioResource(bytes, PluginPlayer.sample_audio_filename)
            }
        }
    }

    private var alreadyDisposed = false

    override fun close() {
        if (alreadyDisposed)
            return
        alreadyDisposed = true
        pluginPlayer.close()
    }

    suspend fun instantiatePlugin() {
        if (!manager.connections.any { it.serviceInfo.packageName == pluginInfo.packageName })
            manager.client.connectToPluginService(pluginInfo.packageName)
        val result = manager.client.instantiateNativePlugin(pluginInfo)
        if (!alreadyDisposed)
            instance.value = result
    }

    fun setNewMidiMappingFlags(pluginId: String, newFlags: Int) {
        AudioPluginMidiSettings.putMidiSettingsToSharedPreference(
            manager.context,
            pluginId,
            newFlags
        )
    }

    fun enableAudioRecorder() {
        pluginPlayer.enableAudioRecorder()
    }

    fun startProcessing() {
        pluginPlayer.startProcessing()
    }

    fun pauseProcessing() {
        pluginPlayer.pauseProcessing()
    }

    fun playPreloadedAudio() {
        pluginPlayer.playPreloadedAudio()
    }

    fun setPresetIndex(index: Int) {
        pluginPlayer.setPresetIndex(index)
    }

    fun setParameterValue(index: Int, value: Float) {
        val ins = instance.value
        if (ins != null)
            pluginPlayer.setParameterValue(ins.getParameter(index).id.toUInt(), value)
    }

    fun processExpression(origin: DiatonicKeyboardNoteExpressionOrigin, note: Int, value: Float) {
        when(origin) {
            DiatonicKeyboardNoteExpressionOrigin.HorizontalDragging -> pluginPlayer.processPitchBend(-1, value)
            DiatonicKeyboardNoteExpressionOrigin.VerticalDragging -> pluginPlayer.processPitchBend(note, value)
            DiatonicKeyboardNoteExpressionOrigin.Pressure -> pluginPlayer.processPressure(note, value)
        }
    }

    fun setNoteState(note: Int, isNoteOn: Boolean) {
        pluginPlayer.setNoteState(note, 0xF800, isNoteOn)
    }
 }

class SurfaceControlUIScope private constructor(private val parentScope: PluginDetailsScope, private val width: Int, private val height: Int) : AutoCloseable {
    companion object {
        fun create(parentScope: PluginDetailsScope, width: Int, height: Int): SurfaceControlUIScope {
            val scope = SurfaceControlUIScope(parentScope, width, height)
            scope.surfaceControl = AudioPluginHostHelper.createSurfaceControl(parentScope.manager.context)
            return scope
        }
    }

    var surfaceControl: AudioPluginSurfaceControlClient? = null

    suspend fun connectSurfaceControlUI() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            val pluginInfo = parentScope.pluginInfo
            surfaceControl!!.connectUINoHandler(
                pluginInfo.packageName,
                pluginInfo.pluginId!!,
                parentScope.instance.value!!.instanceId,
                width,
                height
            )
        }
    }

    fun showSurfaceGUI() {
        surfaceControl?.surfaceView?.visibility = View.VISIBLE
    }

    fun hideSurfaceGUI() {
        surfaceControl?.surfaceView?.visibility = View.GONE
    }

    override fun close() {
        surfaceControl?.close()
    }
}
