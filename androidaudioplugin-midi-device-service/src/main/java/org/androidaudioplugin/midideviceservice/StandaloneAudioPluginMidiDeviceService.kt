package org.androidaudioplugin.midideviceservice

import org.androidaudioplugin.PluginInformation

class StandaloneAudioPluginMidiDeviceService() : AudioPluginMidiDeviceService() {

    override val plugins: List<PluginInformation>
        get() = super.plugins.filter { p -> p.packageName == this.packageName }
}