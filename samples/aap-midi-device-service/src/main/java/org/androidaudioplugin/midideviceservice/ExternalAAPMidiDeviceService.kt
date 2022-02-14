package org.androidaudioplugin.midideviceservice

import org.androidaudioplugin.PluginInformation

class ExternalAAPMidiDeviceService : AudioPluginMidiDeviceService() {
    override fun onCreate() {
        super.onCreate()

        applicationContextForModel = application
    }

    override val plugins: List<PluginInformation>
        get() = model.pluginServices.flatMap { s -> s.plugins }
}
