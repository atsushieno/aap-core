package org.androidaudioplugin.hosting

import android.content.ComponentName
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.content.pm.ServiceInfo
import android.os.Bundle
import android.util.Log
import kotlinx.coroutines.channels.Channel
import kotlinx.coroutines.runBlocking
import org.androidaudioplugin.*
import org.xmlpull.v1.XmlPullParser

class AudioPluginHostHelper {
    companion object {
        const val AAP_ACTION_NAME = "org.androidaudioplugin.AudioPluginService"
        const val AAP_METADATA_NAME_PLUGINS = "org.androidaudioplugin.AudioPluginService#Plugins"
        const val AAP_METADATA_NAME_EXTENSIONS = "org.androidaudioplugin.AudioPluginService#Extensions"
        const val AAP_METADATA_CORE_NS = "urn:org.androidaudioplugin.core"
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
                    } else if (xp.name == "port" && (xp.namespace == "" || xp.namespace == AAP_METADATA_CORE_NS)) {
                        if (currentPlugin != null) {
                            val index = xp.getAttributeValue(null, "index")
                            val name = xp.getAttributeValue(null, "name")
                            val direction = xp.getAttributeValue(null, "direction")
                            val content = xp.getAttributeValue(null, "content")
                            val default = xp.getAttributeValue(AAP_METADATA_PORT_PROPERTIES_NS, "default")
                            val minimum = xp.getAttributeValue(AAP_METADATA_PORT_PROPERTIES_NS, "minimum")
                            val maximum = xp.getAttributeValue(AAP_METADATA_PORT_PROPERTIES_NS, "maximum")
                            val directionInt = if (direction == "input") PortInformation.PORT_DIRECTION_INPUT else PortInformation.PORT_DIRECTION_OUTPUT
                            val contentInt = when (content) {
                                "midi" -> PortInformation.PORT_CONTENT_TYPE_MIDI
                                "midi2" -> PortInformation.PORT_CONTENT_TYPE_MIDI2
                                "audio" -> PortInformation.PORT_CONTENT_TYPE_AUDIO
                                else -> PortInformation.PORT_CONTENT_TYPE_GENERAL
                            }
                            val port = PortInformation(index?.toInt() ?: currentPlugin.ports.size, name, directionInt, contentInt)
                            if (default != null)
                                port.default = default.toFloat()
                            if (minimum != null)
                                port.minimum = minimum.toFloat()
                            if (maximum != null)
                                port.maximum = maximum.toFloat()
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
            val xp = serviceInfo.loadXmlMetaData(context.packageManager, AAP_METADATA_NAME_PLUGINS)
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
                Log.i("AAP", "Service " + serviceInfo.name)
                val pluginServiceInfo = createAudioPluginServiceInformation(context, serviceInfo)
                if (pluginServiceInfo == null) {
                    Log.w(
                        "AAP",
                        "Service " + serviceInfo.name + " has no AAP metadata XML resource."
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
        fun ensureInstanceCreated(servicePackageName: String, connector: AudioPluginServiceConnector) =
            ensureInstanceCreated(queryAudioPluginService(connector.applicationContext, servicePackageName), connector)

        @JvmStatic
        fun ensureInstanceCreated(service: PluginServiceInformation, connector: AudioPluginServiceConnector) {
            val existing = connector.connectedServices.firstOrNull { c -> c.serviceInfo.packageName == service.packageName && c.serviceInfo.className == service.className }
            if (existing != null)
                return

            val waiter = Channel<Unit>()
            runBlocking {
                val listener : (PluginServiceConnection) -> Unit = {
                    suspend {
                        val binder = it.binder
                        if (binder != null)
                            AudioPluginNatives.addBinderForClient(
                                connector.instanceId,
                                service.packageName,
                                service.className,
                                binder
                            )
                        waiter.send(Unit)
                    }
                }
                connector.serviceConnectedListeners.add(listener)
                connector.bindAudioPluginService(service)
                waiter.receive()
            }
        }
    }
}
