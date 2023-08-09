package org.androidaudioplugin.ui.compose.app

import android.content.Context
import android.content.res.AssetManager
import androidx.compose.runtime.snapshots.SnapshotStateList
import androidx.compose.runtime.toMutableStateList
import org.androidaudioplugin.PluginInformation
import org.androidaudioplugin.PluginServiceInformation
import org.androidaudioplugin.composeaudiocontrols.DiatonicKeyboardNoteExpressionOrigin
import org.androidaudioplugin.hosting.AudioPluginClientBase
import org.androidaudioplugin.hosting.AudioPluginInstance
import org.androidaudioplugin.hosting.AudioPluginMidiSettings
import org.androidaudioplugin.hosting.PluginServiceConnection
import org.androidaudioplugin.hosting.UmpHelper
import org.androidaudioplugin.manager.PluginPlayer
import java.nio.IntBuffer

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

    // An observable list version of client.instantiatedPlugins
    val instances = client.instantiatedPlugins.toMutableStateList()

    private val pluginPlayer = PluginPlayer.create().apply {
        context.assets.open(PluginPlayer.sample_audio_filename).use {
            val bytes = ByteArray(it.available())
            it.read(bytes)
            loadAudioResource(bytes, PluginPlayer.sample_audio_filename)
        }
    }

    fun instantiatePlugin(pluginInfo: PluginInformation) {
        val instance = client.instantiatePlugin(pluginInfo)
        instances.add(instance)
    }

    fun setNewMidiMappingFlags(instance: AudioPluginInstance, newFlags: Int) {
        AudioPluginMidiSettings.putMidiSettingsToSharedPreference(
            context,
            instance.pluginInfo.pluginId!!,
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
