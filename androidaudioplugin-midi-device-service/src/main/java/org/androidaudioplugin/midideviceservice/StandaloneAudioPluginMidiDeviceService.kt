package org.androidaudioplugin.midideviceservice

import androidx.annotation.RequiresApi
import org.androidaudioplugin.AudioPluginServiceHelper
import org.androidaudioplugin.PluginInformation

class StandaloneAudioPluginMidiDeviceService : AudioPluginMidiDeviceService() {

    override val plugins: List<PluginInformation>
        get() = AudioPluginServiceHelper.getLocalAudioPluginService(applicationContext)
            .plugins.filter { p -> p.packageName == this.packageName }
}

@RequiresApi(35)
class StandaloneAudioPluginMidiUmpDeviceService : AudioPluginMidiUmpDeviceService() {

    override val plugins: List<PluginInformation>
        get() = AudioPluginServiceHelper.getLocalAudioPluginService(applicationContext)
            .plugins.filter { p -> p.packageName == this.packageName }
}
