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

        init {
            System.loadLibrary("androidaudioplugin")
        }

        @JvmStatic
        fun initialize(context: Context, pluginUris: Array<String>)
        {
            var pluginInfos = queryAudioPluginServices(context).flatMap { i -> i.plugins }.toTypedArray()
            initialize(pluginInfos)
        }

        @JvmStatic
        external fun initialize(pluginInfos: Array<PluginInformation>)

        @JvmStatic
        external fun cleanup()

        // FIXME: remove this sample-only method
//        @JvmStatic
//        external fun runClientAAP(binder: IBinder, sampleRate: Int, pluginId: String, wav: ByteArray, outWav: ByteArray) : Int

        @JvmStatic
        fun queryAudioPluginServices(context: Context): Array<AudioPluginServiceInformation> {
            val intent = Intent(AAP_ACTION_NAME)
            val resolveInfos =
                context.packageManager.queryIntentServices(intent, PackageManager.GET_META_DATA)
            var plugins = mutableListOf<AudioPluginServiceInformation>()
            for (ri in resolveInfos) {
                Log.i("AAP", "Service " + ri.serviceInfo.name)
                // TODO: it is super hacky so far.
                val xp = ri.serviceInfo.loadXmlMetaData(context.packageManager, AAP_ACTION_NAME)
                val aapServiceInfo = AudioPluginServiceInformation(
                    ri.serviceInfo.loadLabel(context.packageManager).toString(),
                    ri.serviceInfo.packageName,
                    ri.serviceInfo.name
                )
                plugins.add(aapServiceInfo)

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
                            val isOutProcess = ri.serviceInfo.packageName != context.packageName
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
            }

            return plugins.toTypedArray()
        }
    }
}
