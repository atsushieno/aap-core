package org.androidaudioplugin

import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.os.IBinder
import android.util.Log
import org.xmlpull.v1.XmlPullParser

class AudioPluginHost {
    companion object {
        const val AAP_ACTION_NAME = "org.androidaudioplugin.AudioPluginService"
        const val AAP_METADATA_NAME_PLUGINS = "org.androidaudioplugin.AudioPluginService#Plugins"
        const val AAP_METADATA_NAME_EXTENSIONS = "org.androidaudioplugin.AudioPluginService#Extensions"

        init {
            System.loadLibrary("androidaudioplugin")
        }

        var initialized: Boolean = false

        @JvmStatic
        fun initialize(context: Context)
        {
            var si = getLocalAudioPluginService(context)
            si.extensions.forEach { e ->
                if(e == null || e == "")
                    return
                var c = Class.forName(e)
                if (c == null) {
                    Log.d("AAP", "Extension class not found: " + e)
                    return@forEach
                }
                var method = c.getMethod("initialize", Context::class.java)
                method.invoke(null, context)
            }
            var pluginInfos = queryAudioPluginServices(context).flatMap { i -> i.plugins }.toTypedArray()
            initialize(pluginInfos)
            initialized = true
        }

        @JvmStatic
        external fun initialize(pluginInfos: Array<PluginInformation>)

        @JvmStatic
        fun cleanup()
        {
            cleanupNatives()
            initialized = false
        }

        @JvmStatic
        external fun cleanupNatives()

        private fun parseAapMetadata(isOutProcess: Boolean, name: String, packageName: String, className: String, xp: XmlPullParser) : AudioPluginServiceInformation {
            // TODO: this XML parsing is super hacky so far.
            val aapServiceInfo = AudioPluginServiceInformation(name, packageName, className)

            var currentPlugin: PluginInformation? = null
            while (true) {
                var eventType = xp.next()
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
                        val isOutProcess = isOutProcess
                        currentPlugin = PluginInformation(
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
                            isOutProcess
                        )
                        aapServiceInfo.plugins.add(currentPlugin)
                    } else if (xp.name == "port") {
                        if (currentPlugin != null) {
                            val name = xp.getAttributeValue(null, "name")
                            val direction = xp.getAttributeValue(null, "direction")
                            val content = xp.getAttributeValue(null, "content")
                            val port = PortInformation(
                                name,
                                if (direction == "input") PortInformation.PORT_DIRECTION_INPUT else PortInformation.PORT_DIRECTION_OUTPUT,
                                if (content == "midi") PortInformation.PORT_CONTENT_TYPE_MIDI else if (content == "audio") PortInformation.PORT_CONTENT_TYPE_AUDIO else PortInformation.PORT_CONTENT_TYPE_GENERAL
                            )
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

        @JvmStatic
        fun queryAudioPluginServices(context: Context): Array<AudioPluginServiceInformation> {
            val intent = Intent(AAP_ACTION_NAME)
            val resolveInfos =
                context.packageManager.queryIntentServices(intent, PackageManager.GET_META_DATA)
            var plugins = mutableListOf<AudioPluginServiceInformation>()
            for (ri in resolveInfos) {
                Log.i("AAP", "Service " + ri.serviceInfo.name)
                val xp = ri.serviceInfo.loadXmlMetaData(context.packageManager, AAP_METADATA_NAME_PLUGINS)
                if (xp == null) {
                    Log.w("AAP", "Service " + ri.serviceInfo.name + " has no AAP metadata XML resource.")
                    continue
                }
                var isOutProcess = ri.serviceInfo.packageName != context.packageName
                var name = ri.serviceInfo.loadLabel(context.packageManager).toString()
                var packageName = ri.serviceInfo.packageName
                var className = ri.serviceInfo.name
                var plugin = parseAapMetadata(isOutProcess, name, packageName, className, xp)
                var extensions = ri.serviceInfo.metaData.getString(AAP_METADATA_NAME_EXTENSIONS)
                if (extensions != null)
                    plugin.extensions = extensions.toString().split(',').toMutableList()
                plugins.add(plugin)
            }

            return plugins.toTypedArray()
        }

        @JvmStatic
        fun getLocalAudioPluginService(context: Context) = queryAudioPluginServices(context).first { svc -> svc.packageName == context.packageName}
    }
}
