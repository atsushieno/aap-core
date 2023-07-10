package org.androidaudioplugin.ui.compose.app

import android.content.Context
import androidx.compose.runtime.snapshots.SnapshotStateList
import androidx.compose.runtime.toMutableStateList
import org.androidaudioplugin.PluginInformation
import org.androidaudioplugin.PluginServiceInformation
import org.androidaudioplugin.hosting.AudioPluginClientBase
import org.androidaudioplugin.hosting.AudioPluginInstance
import org.androidaudioplugin.hosting.AudioPluginMidiSettings
import org.androidaudioplugin.hosting.PluginServiceConnection

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
}