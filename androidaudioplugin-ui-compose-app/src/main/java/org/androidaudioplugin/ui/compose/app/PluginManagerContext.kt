package org.androidaudioplugin.ui.compose.app

import android.content.Context
import androidx.compose.runtime.snapshots.SnapshotStateList
import androidx.compose.runtime.toMutableStateList
import org.androidaudioplugin.PluginInformation
import org.androidaudioplugin.PluginServiceInformation
import org.androidaudioplugin.composeaudiocontrols.DiatonicKeyboardNoteExpressionOrigin
import org.androidaudioplugin.hosting.AudioPluginClientBase
import org.androidaudioplugin.hosting.AudioPluginMidiSettings
import org.androidaudioplugin.hosting.NativeRemotePluginInstance
import org.androidaudioplugin.hosting.PluginServiceConnection
import org.androidaudioplugin.manager.PluginPlayer

class PluginManagerContext(val context: Context,
                           val pluginServices: SnapshotStateList<PluginServiceInformation>
) {
    val logTag = "AAPPluginManager"

    val client = AudioPluginClientBase(context).apply {
        onConnectedListeners.add { conn -> connections.add(conn) }
        onDisconnectingListeners.add { conn -> connections.remove(conn) }
    }

    // An observable list version of service connections
    val connections = listOf<PluginServiceConnection>().toMutableStateList()

    var instance: NativeRemotePluginInstance? = null

    private val pluginPlayer = PluginPlayer.create().apply {
        context.assets.open(PluginPlayer.sample_audio_filename).use {
            val bytes = ByteArray(it.available())
            it.read(bytes)
            loadAudioResource(bytes, PluginPlayer.sample_audio_filename)
        }
    }

    fun instantiatePlugin(pluginInfo: PluginInformation) {
        instance = client.instantiateNativePlugin(pluginInfo)
    }

    fun setNewMidiMappingFlags(pluginId: String, newFlags: Int) {
        AudioPluginMidiSettings.putMidiSettingsToSharedPreference(
            context,
            pluginId,
            newFlags
        )
    }

    fun playPreloadedAudio() {
        pluginPlayer.playPreloadedAudio()
    }

    fun setParameterValue(parameterId: UInt, value: Float) {
        pluginPlayer.setParameterValue(parameterId, value)
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
