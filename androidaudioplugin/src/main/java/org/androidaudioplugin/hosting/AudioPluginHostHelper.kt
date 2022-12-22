package org.androidaudioplugin.hosting

import android.content.ComponentName
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.content.pm.ServiceInfo
import android.os.Bundle
import android.util.Log
import kotlinx.coroutines.runBlocking
import org.androidaudioplugin.*
import org.xmlpull.v1.XmlPullParser

class AudioPluginHostHelper {

    class AAPMetadataException : Exception {
        constructor() : super("Error in AAP metadata")
        constructor(message: String) : super(message)
        constructor(message: String, innerException: Exception) : super(message, innerException)
    }

    companion object {
        const val AAP_ACTION_NAME = "org.androidaudioplugin.AudioPluginService.V2"
        const val AAP_METADATA_NAME_PLUGINS = "org.androidaudioplugin.AudioPluginService.V2#Plugins"
        const val AAP_METADATA_NAME_EXTENSIONS = "org.androidaudioplugin.AudioPluginService.V2#Extensions"
        const val AAP_METADATA_CORE_NS = "urn:org.androidaudioplugin.core"
        const val AAP_METADATA_EXT_PARAMETERS_NS = "urn://androidaudioplugin.org/extensions/parameters"
        const val AAP_METADATA_PORT_PROPERTIES_NS = "urn:org.androidaudioplugin.port"

        private fun parseAapMetadata(isOutProcess: Boolean, label: String, packageName: String, className: String, xp: XmlPullParser) : PluginServiceInformation {
            val aapServiceInfo = PluginServiceInformation(label, packageName, className)

            var currentPlugin: PluginInformation? = null
            while (true) {
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
                        val backend = xp.getAttributeValue(null, "backend")
                        val version = xp.getAttributeValue(null, "version")
                        val category = xp.getAttributeValue(null, "category")
                        val author = xp.getAttributeValue(null, "author")
                        val manufacturer = xp.getAttributeValue(null, "manufacturer")
                        val uniqueId = xp.getAttributeValue(null, "unique-id")
                        val sharedLibraryName = xp.getAttributeValue(null, "library")
                        val libraryEntryPoint = xp.getAttributeValue(null, "entrypoint")
                        val assets = xp.getAttributeValue(null, "assets")
                        val uiActivity = xp.getAttributeValue(null, "ui-activity")
                        val uiWeb = xp.getAttributeValue(null, "ui-web")
                        currentPlugin = PluginInformation(
                            packageName,
                            className,
                            name,
                            backend,
                            version,
                            category,
                            author,
                            manufacturer,
                            uniqueId,
                            sharedLibraryName,
                            libraryEntryPoint,
                            assets,
                            uiActivity,
                            uiWeb,
                            isOutProcess
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
                            val id: String? = xp.getAttributeValue(null, "id")
                            if (id == null)
                                throw AAPMetadataException("Mandatory attribute \"id\" is missing on <port> element. (line ${xp.lineNumber}, column ${xp.columnNumber})")
                            if (id.toIntOrNull() == null)
                                throw AAPMetadataException("The \"id\" attribute on a <port> element must be a valid integer. (line ${xp.lineNumber}, column ${xp.columnNumber})")
                            // Android Bug: XmlPullParser.getAttributeValue() returns null, whereas Kotlinized API claims that it returns non-null!
                            val name: String? = xp.getAttributeValue(null, "name")
                            if (name == null)
                                throw AAPMetadataException("Mandatory attribute \"name\" is missing on <port> element. (line ${xp.lineNumber}, column ${xp.columnNumber})")
                            val default = xp.getAttributeValue(null, "default")
                            val minimum = xp.getAttributeValue(null, "minimum")
                            val maximum = xp.getAttributeValue(null, "maximum")
                            // FIXME: handle parse errors gracefully
                            val para = ParameterInformation(id.toInt(), name,
                                minimum?.toDouble() ?: 0.0,
                                maximum?.toDouble() ?: 1.0,
                                default?.toDouble() ?: 0.0
                            )
                            currentPlugin.parameters.add(para)
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
                                port.minimumSizeInBytes = minimumSize.toInt()
                            currentPlugin.ports.add(port)
                        }
                    }
                }
                if (eventType == XmlPullParser.END_TAG) {
                    if (xp.name == "plugin" && (xp.namespace == "" || xp.namespace == AAP_METADATA_CORE_NS))
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
        fun launchInProcessPluginUI(context: Context, pluginInfo: PluginInformation, instanceId: Int?) {
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
                    connector.applicationContext,
                    servicePackageName
                ), connector
            )
        }

        @JvmStatic
        fun ensureBinderConnected(service: PluginServiceInformation, connector: AudioPluginServiceConnector) {
            val existing = connector.connectedServices.firstOrNull { c -> c.serviceInfo.packageName == service.packageName && c.serviceInfo.className == service.className }
            if (existing != null)
                return

            runBlocking {
                val conn = connector.bindAudioPluginService(service)
                AudioPluginNatives.addBinderForClient(
                    connector.instanceId,
                    service.packageName,
                    service.className,
                    conn.binder
                )
            }
        }
    }
}
