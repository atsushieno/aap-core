package org.androidaudioplugin.midideviceservice

import org.androidaudioplugin.PluginInformation

class StandaloneAudioPluginMidiDeviceService() : AudioPluginMidiDeviceService() {

    override val plugins: List<PluginInformation>
        get() = super.plugins.filter { p -> p.packageName == this.packageName }

    override fun getPluginId(portIntex: Int): String =
        plugins[portIntex].pluginId!!
}