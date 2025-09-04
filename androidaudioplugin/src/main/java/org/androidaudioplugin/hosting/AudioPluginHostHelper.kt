package org.androidaudioplugin.hosting

import android.content.ComponentName
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.content.pm.ServiceInfo
import android.os.Bundle
import android.util.Log
import kotlinx.coroutines.runBlocking
import org.androidaudioplugin.AudioPluginNatives
import org.androidaudioplugin.AudioPluginService
import org.androidaudioplugin.ExtensionInformation
import org.androidaudioplugin.ParameterInformation
import org.androidaudioplugin.PluginInformation
import org.androidaudioplugin.PluginServiceInformation
import org.androidaudioplugin.PortInformation
import org.xmlpull.v1.XmlPullParser

object AudioPluginHostHelper {

    const val AAP_ACTION_NAME = "org.androidaudioplugin.AudioPluginService.V3"
    const val AAP_METADATA_NAME_PLUGINS = "$AAP_ACTION_NAME#Plugins"
    const val AAP_METADATA_NAME_EXTENSIONS = "$AAP_ACTION_NAME#Extensions"
    const val AAP_METADATA_CORE_NS = "urn:org.androidaudioplugin.core"
    const val AAP_METADATA_EXT_PARAMETERS_NS = "urn://androidaudioplugin.org/extensions/parameters"
    const val AAP_METADATA_PORT_PROPERTIES_NS = "urn:org.androidaudioplugin.port"
    const val AAP_METADATA_EXT_GUI_NS = "urn://androidaudioplugin.org/extensions/gui"

    private fun logMetadataError(error: AAPMetadataException) {
        Log.w("AudioPluginHostHelper", error.toString())
    }

    private fun parseAapMetadata(isOutProcess: Boolean, label: String, packageName: String, className: String, xp: XmlPullParser, onError: (AAPMetadataException) -> Unit = { logMetadataError(it) }) : PluginServiceInformation {
        val aapServiceInfo = PluginServiceInformation(label, packageName, className)

        var currentPlugin: PluginInformation? = null
        var currentParameter: ParameterInformation? = null
        val safeDouble: (Double) -> Double = { v -> if (v.isFinite()) v else 0.0 }
        while (true) {
        try {
            val eventType = xp.next()
            if (eventType == XmlPullParser.END_DOCUMENT)
                break
            if (eventType == XmlPullParser.IGNORABLE_WHITESPACE)
                continue
            if (eventType == XmlPullParser.START_TAG) {
                if (xp.name == "plugin" && (xp.namespace == "" || xp.namespace == AAP_METADATA_CORE_NS)) {
                    if (currentPlugin != null)
                        continue
                    val name = xp.getAttributeValue(null, "name")
                    val version = xp.getAttributeValue(null, "version")
                    val category = xp.getAttributeValue(null, "category")
                    val developer = xp.getAttributeValue(null, "developer")
                    val uniqueId = xp.getAttributeValue(null, "unique-id")
                    val sharedLibraryName = xp.getAttributeValue(null, "library")
                    val libraryEntryPoint = xp.getAttributeValue(null, "entrypoint")
                    val uiViewFactory = xp.getAttributeValue(AAP_METADATA_EXT_GUI_NS, "ui-view-factory")
                    val uiActivity = xp.getAttributeValue(AAP_METADATA_EXT_GUI_NS, "ui-activity")
                    val uiWeb = xp.getAttributeValue(AAP_METADATA_EXT_GUI_NS, "ui-web")
                    currentPlugin = PluginInformation(
                        packageName,
                        className,
                        name,
                        version,
                        category,
                        developer,
                        uniqueId,
                        sharedLibraryName,
                        libraryEntryPoint,
                        uiViewFactory,
                        uiActivity,
                        uiWeb,
                        isOutProcess,
                    )
                    aapServiceInfo.plugins.add(currentPlugin)
                } else if (xp.name == "extension" && (xp.namespace == "" || xp.namespace == AAP_METADATA_CORE_NS)) {
                    if (currentPlugin != null) {
                        val required = xp.getAttributeValue(null, "required")
                        val name = xp.getAttributeValue(null, "uri")
                        val extension = ExtensionInformation(required.toBoolean(), name)
                        currentPlugin.extensions.add(extension)
                    }
                } else if (xp.name == "parameter" && (xp.namespace == "" || xp.namespace == AAP_METADATA_EXT_PARAMETERS_NS)) {
                    if (currentPlugin != null) {
                        // Android Bug: XmlPullParser.getAttributeValue() returns null, whereas Kotlinized API claims that it returns non-null!
                        val id: String = xp.getAttributeValue(null, "id")
                            ?: throw AAPMetadataException("Mandatory attribute \"id\" is missing on <port> element. (line ${xp.lineNumber}, column ${xp.columnNumber})")
                        if (id.toIntOrNull() == null)
                            throw AAPMetadataException("The \"id\" attribute on a <port> element must be a valid integer. (line ${xp.lineNumber}, column ${xp.columnNumber})")
                        // Android Bug: XmlPullParser.getAttributeValue() returns null, whereas Kotlinized API claims that it returns non-null!
                        val name: String = xp.getAttributeValue(null, "name")
                            ?: throw AAPMetadataException("Mandatory attribute \"name\" is missing on <port> element. (line ${xp.lineNumber}, column ${xp.columnNumber})")
                        val default = xp.getAttributeValue(null, "default")
                        val minimum = xp.getAttributeValue(null, "minimum")
                        val maximum = xp.getAttributeValue(null, "maximum")
                        currentParameter = ParameterInformation(id.toInt(), name,
                            safeDouble(minimum?.toDouble() ?: 0.0),
                            safeDouble(maximum?.toDouble() ?: 1.0),
                            safeDouble(default?.toDouble() ?: 0.0)
                        )
                        currentPlugin.parameters.add(currentParameter)
                    }
                } else if (xp.name == "port" && (xp.namespace == "" || xp.namespace == AAP_METADATA_CORE_NS)) {
                    if (currentPlugin != null) {
                        val index = xp.getAttributeValue(null, "index")
                        val name = xp.getAttributeValue(null, "name")
                        val direction = xp.getAttributeValue(null, "direction")
                        val content = xp.getAttributeValue(null, "content")
                        val minimumSize =
                            xp.getAttributeValue(AAP_METADATA_PORT_PROPERTIES_NS, "minimumSize")
                        val directionInt =
                            if (direction == "input") PortInformation.PORT_DIRECTION_INPUT else PortInformation.PORT_DIRECTION_OUTPUT
                        val contentInt = when (content) {
                            "midi" -> PortInformation.PORT_CONTENT_TYPE_MIDI
                            "midi2" -> PortInformation.PORT_CONTENT_TYPE_MIDI2
                            "audio" -> PortInformation.PORT_CONTENT_TYPE_AUDIO
                            else -> PortInformation.PORT_CONTENT_TYPE_GENERAL
                        }
                        val port = PortInformation(
                            index?.toInt() ?: currentPlugin.ports.size,
                            name,
                            directionInt,
                            contentInt
                        )
                        if (minimumSize != null)
                            port.minimumSizeInBytes = minimumSize.toIntOrNull()
                                ?: throw AAPMetadataException("The \"minimumSize\" attribute on a <port> element must be a valid integer. (line ${xp.lineNumber}, column ${xp.columnNumber})")
                        currentPlugin.ports.add(port)
                    }
                } else if (xp.name == "enumeration" && xp.namespace == AAP_METADATA_EXT_PARAMETERS_NS) {
                    if (currentParameter != null) {
                        val value = xp.getAttributeValue(null, "value")?.toDoubleOrNull() ?: throw AAPMetadataException("A mandatory attribute `value` is missing or invalid double value for an `enumeration` element (line ${xp.lineNumber}, column ${xp.columnNumber})\"")
                        val name = xp.getAttributeValue(null, "name")
                        if (name == null || name.isEmpty())
                            Log.w("AudioPluginHostHelper", "A mandatory attribute `name` is missing or empty on an `enumeration` element (line ${xp.lineNumber}, column ${xp.columnNumber})\"")
                        else
                            currentParameter.enumerations.add(ParameterInformation.EnumerationInformation(currentParameter.enumerations.size, value, name))
                    }
                }
            }
            if (eventType == XmlPullParser.END_TAG) {
                if (xp.name == "plugin" && (xp.namespace == "" || xp.namespace == AAP_METADATA_CORE_NS))
                    currentPlugin = null
                if (xp.name == "parameter" && (xp.namespace == "" || xp.namespace == AAP_METADATA_EXT_PARAMETERS_NS))
                    currentParameter = null
            }
        } catch (error: AAPMetadataException) {
            onError(error)
            // invalidate contexts
            if (currentParameter != null)
                currentParameter = null
            else if (currentPlugin != null)
                currentPlugin = null
        }
        }
        return aapServiceInfo
    }

    // Not sure if this is a good idea. It is used by native code because we are not sure if
    // expanding kotlin list operators in JNI code is good idea, but we don't want to have
    // methods like this a lot...
    @JvmStatic
    fun queryAudioPlugins(context: Context): Array<PluginInformation> {
        return queryAudioPluginServices(context).flatMap { s -> s.plugins }.toTypedArray()
    }

    @JvmStatic
    fun createAudioPluginServiceInformation(context: Context, serviceInfo: ServiceInfo) : PluginServiceInformation? {
        try {
            val xp =
                serviceInfo.loadXmlMetaData(context.packageManager, AAP_METADATA_NAME_PLUGINS)
                    ?: return null
            val isOutProcess = serviceInfo.packageName != context.packageName
            val label = serviceInfo.loadLabel(context.packageManager).toString()
            val packageName = serviceInfo.packageName
            val className = serviceInfo.name
            val plugin = parseAapMetadata(isOutProcess, label, packageName, className, xp)
            if (serviceInfo.icon != 0)
                plugin.icon = serviceInfo.loadIcon(context.packageManager)
            if (plugin.icon == null && serviceInfo.applicationInfo.icon != 0)
                plugin.icon = serviceInfo.applicationInfo.loadIcon(context.packageManager)
            val extensions = serviceInfo.metaData.getString(AAP_METADATA_NAME_EXTENSIONS)
            if (extensions != null)
                plugin.extensions = extensions.toString().split(',').toMutableList()
            return plugin
        } catch (ex: AAPMetadataException) {
            Log.e("AAP", "Failed to load AAP metadata for ${serviceInfo.packageName}/${serviceInfo.name}: ${ex.message}")
            return null
        }
    }

    @JvmStatic
    fun queryAudioPluginService(context: Context, packageName: String) =
        queryAudioPluginServices(context, packageName).first()

    @JvmStatic
    fun queryAudioPluginServices(context: Context, packageNameFilter: String? = null): Array<PluginServiceInformation> {
        val intent = Intent(AAP_ACTION_NAME)
        if (packageNameFilter != null)
            intent.setPackage(packageNameFilter)
        val resolveInfos =
            context.packageManager.queryIntentServices(intent, PackageManager.GET_META_DATA)
        val plugins = mutableListOf<PluginServiceInformation>()
        for (ri in resolveInfos) {
            val serviceInfo = ri.serviceInfo
            val pluginServiceInfo = createAudioPluginServiceInformation(context, serviceInfo)
            if (pluginServiceInfo == null) {
                Log.w(
                    "AAP",
                    "Audio Plugin Service \"" + serviceInfo.name + "\" is found, but with no readable AAP metadata XML resource. Ignored."
                )
                continue
            }
            plugins.add(pluginServiceInfo)
        }

        return plugins.toTypedArray()
    }

    @JvmStatic
    fun launchPluginUIActivity(context: Context, pluginInfo: PluginInformation, instanceId: Int?) {
        val pkg = pluginInfo.packageName
        val pkgInfo = context.packageManager.getPackageInfo(pluginInfo.packageName, PackageManager.GET_ACTIVITIES)
        val activity = pluginInfo.uiActivity ?: pkgInfo.activities?.first()?.name ?: ".MainActivity"
        // If current plugin instance in this app is of this plugin, then use it as the target instance.
        val intent = Intent()
        intent.component = ComponentName(pkg, activity)
        val bundle = Bundle()
        if (instanceId != null)
            intent.putExtra("instanceId", instanceId.toInt())
        context.startActivity(intent, bundle)
    }

    @Deprecated("Use AudioPluginServiceHelper.getLocalAudioPluginService()",
        ReplaceWith("AudioPluginServiceHelper.getLocalAudioPluginService(context)", "org.androidaudioplugin"))
    @JvmStatic
    fun getLocalAudioPluginService(context: Context) : PluginServiceInformation {
        val componentName = ComponentName(context.packageName, AudioPluginService::class.java.name)
        val serviceInfo = context.packageManager.getServiceInfo(componentName, PackageManager.GET_META_DATA)
        return createAudioPluginServiceInformation(context, serviceInfo)!!
    }

    @JvmStatic
    fun ensureBinderConnected(servicePackageName: String, connector: AudioPluginServiceConnector) {
        ensureBinderConnected(
            queryAudioPluginService(
                connector.context,
                servicePackageName
            ), connector
        )
    }

    @JvmStatic
    fun ensureBinderConnected(service: PluginServiceInformation, connector: AudioPluginServiceConnector) {
        val existing = connector.findExistingServiceConnection(service.packageName)
        if (existing != null)
            return

        runBlocking {
            val conn = connector.bindAudioPluginService(service)
            AudioPluginNatives.addBinderForClient(
                connector.serviceConnectionId,
                service.packageName,
                service.className,
                conn.binder
            )
        }
    }

    @JvmStatic
    fun createSurfaceControl(context: Context) = AudioPluginSurfaceControlClient(context)
}

