package org.androidaudiopluginframework

class AudioPluginServiceInformation(name: String, packageName: String, className: String)
{
    var name = name
    var packageName = packageName
    var className = className
    var plugins = mutableListOf<PluginInformation>()
}

class PluginInformation(name: String?, backend: String?, version: String?, category: String?, author: String?, manufacturer: String?, pluginId: String?, sharedLibraryName: String?, libraryEntryPoint: String?, isOutProcess: Boolean)
{
    var name : String? = name
    var backend : String? = backend
    var version : String? = version
    var category : String? = category
    var author : String? = author
    var manufacturer : String? = manufacturer
    var pluginId : String? = pluginId
    var sharedLibraryName : String? = sharedLibraryName
    var libraryEntryPoint : String? = libraryEntryPoint
    var isOutProcess : Boolean = isOutProcess
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
