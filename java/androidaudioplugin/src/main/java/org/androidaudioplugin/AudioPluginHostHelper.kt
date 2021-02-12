package org.androidaudioplugin

import android.content.ComponentName
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.content.pm.ServiceInfo
import android.os.Bundle
import android.os.IBinder
import android.util.Log
import org.xmlpull.v1.XmlPullParser

class AudioPluginHostHelper {
    companion object {
        const val AAP_ACTION_NAME = "org.androidaudioplugin.AudioPluginService"
        const val AAP_METADATA_NAME_PLUGINS = "org.androidaudioplugin.AudioPluginService#Plugins"
        const val AAP_METADATA_NAME_EXTENSIONS = "org.androidaudioplugin.AudioPluginService#Extensions"

        private fun parseAapMetadata(isOutProcess: Boolean, label: String, packageName: String, className: String, xp: XmlPullParser) : AudioPluginServiceInformation {
            // TODO: this XML parsing is super hacky so far.
            val aapServiceInfo = AudioPluginServiceInformation(label, packageName, className)

            var currentPlugin: PluginInformation? = null
            while (true) {
                val eventType = xp.next()
                if (eventType == XmlPullParser.END_DOCUMENT)
                    break
                if (eventType == XmlPullParser.IGNORABLE_WHITESPACE)
                    continue
                if (eventType == XmlPullParser.START_TAG) {
                    if (xp.name == "plugin") {
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
                    } else if (xp.name == "port") {
                        if (currentPlugin != null) {
                            val name = xp.getAttributeValue(null, "name")
                            val direction = xp.getAttributeValue(null, "direction")
                            val content = xp.getAttributeValue(null, "content")
                            val default = xp.getAttributeValue(null, "default")
                            val minimum = xp.getAttributeValue(null, "minimum")
                            val maximum = xp.getAttributeValue(null, "maximum")
                            val port = PortInformation(
                                name,
                                if (direction == "input") PortInformation.PORT_DIRECTION_INPUT else PortInformation.PORT_DIRECTION_OUTPUT,
                                if (content == "midi") PortInformation.PORT_CONTENT_TYPE_MIDI else if (content == "audio") PortInformation.PORT_CONTENT_TYPE_AUDIO else PortInformation.PORT_CONTENT_TYPE_GENERAL
                            )
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
                    if (xp.name == "plugin")
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
        fun createAudioPluginServiceInformation(context: Context, serviceInfo: ServiceInfo) : AudioPluginServiceInformation? {
            val xp = serviceInfo.loadXmlMetaData(context.packageManager, AAP_METADATA_NAME_PLUGINS)
                ?: return null
            val isOutProcess = serviceInfo.packageName != context.packageName
            val label = serviceInfo.loadLabel(context.packageManager).toString()
            val packageName = serviceInfo.packageName
            val className = serviceInfo.name
            val plugin = parseAapMetadata(isOutProcess, label, packageName, className, xp)
            val extensions = serviceInfo.metaData.getString(AAP_METADATA_NAME_EXTENSIONS)
            if (extensions != null)
                plugin.extensions = extensions.toString().split(',').toMutableList()
            return plugin
        }

        @JvmStatic
        fun queryAudioPluginServices(context: Context): Array<AudioPluginServiceInformation> {
            val intent = Intent(AAP_ACTION_NAME)
            val resolveInfos =
                context.packageManager.queryIntentServices(intent, PackageManager.GET_META_DATA)
            val plugins = mutableListOf<AudioPluginServiceInformation>()
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
    }
}
