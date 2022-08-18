package org.androidaudioplugin

/**
 * Plugin information structure. The members mostly correspond to attributes and content elements
 * in a `<plugin>` element in `aap_metadata.xml`.
 */
class PluginInformation(var packageName: String, var localName: String, var displayName: String, var backend: String?, var version: String?,
                        var category: String?, var author: String?, var manufacturer: String?,
                        var pluginId: String?, var sharedLibraryName: String?,
                        var libraryEntryPoint: String?, var assets: String?, var uiActivity : String?, var uiWeb : String?, var isOutProcess: Boolean)
{
    companion object {
        const val PRIMARY_CATEGORY_EFFECT = "Effect"
        const val PRIMARY_CATEGORY_INSTRUMENT = "Instrument"
    }

    var extensions = mutableListOf<ExtensionInformation>()

    var parameters = mutableListOf<ParameterInformation>()

    var ports = mutableListOf<PortInformation>()

    // These obvious members are for use in C interop and not expected to be consumed by user developers.
    fun getExtensionCount() : Int
    {
        return extensions.size
    }
    fun getExtension(index : Int) : ExtensionInformation
    {
        return extensions[index]
    }
    fun getDeclaredParameterCount() : Int
    {
        return parameters.size
    }
    fun getDeclaredParameter(index : Int) : ParameterInformation {
        return parameters[index]
    }
    fun getDeclaredPortCount() : Int
    {
        return ports.size
    }
    fun getDeclaredPort(index : Int) : PortInformation {
        return ports[index]
    }
}