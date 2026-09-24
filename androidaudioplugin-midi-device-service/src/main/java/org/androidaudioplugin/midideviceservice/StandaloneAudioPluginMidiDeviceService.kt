package org.androidaudioplugin.midideviceservice

import android.content.Context
import androidx.annotation.RequiresApi
import org.androidaudioplugin.AudioPluginServiceHelper
import org.androidaudioplugin.PluginInformation

// A plugin package may have more than one AudioPluginService (in separate processes).
private fun getLocalPlugins(context: Context): List<PluginInformation> =
    AudioPluginServiceHelper.getLocalAudioPluginServices(context)
        .flatMap { it.plugins }
        .distinctBy { it.pluginId }

class StandaloneAudioPluginMidiDeviceService : AudioPluginMidiDeviceService() {

    override val plugins: List<PluginInformation>
        get() = getLocalPlugins(applicationContext)
}

@RequiresApi(35)
class StandaloneAudioPluginMidiUmpDeviceService : AudioPluginMidiUmpDeviceService() {

    override val plugins: List<PluginInformation>
        get() = getLocalPlugins(applicationContext)
}
