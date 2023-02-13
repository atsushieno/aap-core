package org.androidaudioplugin

/**
 * Plugin information structure. The members mostly correspond to attributes and content elements
 * in a `<plugin>` element in `aap_metadata.xml`.
 */
class PluginInformation(
    /** Android package name */
    var packageName: String,
    /** Android class name */
    var localName: String,
    /** human readable name of the plugin */
    var displayName: String,
    /** human readable version code */
    var version: String?,
    /** category label. Right now `Instrument` or `Effect` are the expected values. */
    var category: String?,
    /** developer name (such as personal name, company name, team name) */
    var developer: String?,
    /** plugin identifier, typically a URN */
    var pluginId: String?,
    /** the shared library name that needs to be loaded when AudioPluginService is initialized. */
    var sharedLibraryName: String?,
    /** AAP factory entrypoint function name */
    var libraryEntryPoint: String?,
    /** in-plugin-process UI View factory class name */
    var uiViewFactory: String? = null,
    /** in-plugin-process UI Activity class name */
    var uiActivity : String? = null,
    /** in-host-process Web UI archive name */
    var uiWeb : String? = null,
    /** indicates if it is in-process plugin or not */
    var isOutProcess: Boolean = true)
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