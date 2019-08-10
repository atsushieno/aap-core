package org.androidaudioplugin

class AudioPluginServiceInformation(var name: String, var packageName: String,
                                    var className: String
)
{
    var plugins = mutableListOf<PluginInformation>()
}

class PluginInformation(var name: String?, var backend: String?, var version: String?,
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

class PortInformation(var name: String, var direction: Int, var content: Int)
{
    companion object {
        const val PORT_DIRECTION_INPUT = 0
        const val PORT_DIRECTION_OUTPUT = 1

        const val PORT_CONTENT_TYPE_GENERAL = 0
        const val PORT_CONTENT_TYPE_AUDIO = 1
        const val PORT_CONTENT_TYPE_MIDI = 2
    }
}
