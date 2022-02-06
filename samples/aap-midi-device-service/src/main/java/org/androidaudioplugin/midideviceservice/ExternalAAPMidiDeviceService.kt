package org.androidaudioplugin.midideviceservice

import org.androidaudioplugin.PluginInformation

class ExternalAAPMidiDeviceService : AudioPluginMidiDeviceService() {
    override fun onCreate() {
        super.onCreate()

        applicationContextForModel = application
    }

    override fun getPluginId(portIntex: Int): String {
        assert(portIntex == 0)
        // Port index does not matter, as it is only to designate the input port of the device
        // in "device info xml" meta-data. What we need here is the plugin ID of the selected item.
        return model.specifiedInstrument!!.pluginId!!
    }

    override val plugins: List<PluginInformation>
        get() = model.pluginServices.flatMap { s -> s.plugins }
}
