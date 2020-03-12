package org.androidaudioplugin

class PluginInformation(var serviceIdentifier: String, var displayName: String, var backend: String?, var version: String?,
                        var category: String?, var author: String?, var manufacturer: String?,
                        var pluginId: String?, var sharedLibraryName: String?,
                        var libraryEntryPoint: String?, var assets: String?, var isOutProcess: Boolean)
{
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