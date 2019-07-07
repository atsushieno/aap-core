package org.androidaudiopluginframework

class AudioPluginServiceInformation(var name: String, var packageName: String,
                                    var className: String
)
{
    var plugins = mutableListOf<PluginInformation>()
}

class PluginInformation(var name: String?, var backend: String?, var version: String?,
                        var category: String?, var author: String?, var manufacturer: String?,
                        var pluginId: String?, var sharedLibraryName: String?,
                        var libraryEntryPoint: String?, var isOutProcess: Boolean)
{
    var ports = mutableListOf<PortInformation>()
}

class PortInformation(name: String, direction: Int, content: Int)
{
    companion object {
        const val PORT_DIRECTION_INPUT = 0
        const val PORT_DIRECTION_OUTPUT = 1

        const val PORT_CONTENT_TYPE_GENERAL = 0
        const val PORT_CONTENT_TYPE_AUDIO = 1
        const val PORT_CONTENT_TYPE_MIDI = 2
    }

    val name : String = name
    val direction : Int = direction
    val content : Int = content
}
