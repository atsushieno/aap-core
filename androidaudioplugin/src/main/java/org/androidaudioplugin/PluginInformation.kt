package org.androidaudioplugin

class PluginInformation(var packageName: String, var localName: String, var displayName: String, var backend: String?, var version: String?,
                        var category: String?, var author: String?, var manufacturer: String?,
                        var pluginId: String?, var sharedLibraryName: String?,
                        var libraryEntryPoint: String?, var assets: String?, var uiActivity : String?, var uiWeb : String?, var isOutProcess: Boolean)
{
    companion object {
        const val PRIMARY_CATEGORY_EFFECT = "Effect"
        const val PRIMARY_CATEGORY_INSTRUMENT = "Instrument"
    }

    var ports = mutableListOf<PortInformation>()

    // These obvious members are for use in C interop.
    fun getPortCount() : Int
    {
        return ports.size
    }

    fun getPort(index : Int) : PortInformation
    {
        return ports[index]
    }
}