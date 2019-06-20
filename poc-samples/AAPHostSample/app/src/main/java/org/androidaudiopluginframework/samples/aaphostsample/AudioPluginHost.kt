package org.androidaudiopluginframework

import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.media.midi.MidiDeviceInfo
import android.util.Log
import org.xmlpull.v1.XmlPullParser

class AudioPluginHost
{
    companion object {
        const val AAP_ACTION_NAME = "org.androidaudiopluginframework.AudioPluginService"
    }

    fun queryAudioPluginServices(context: Context) : Array<AudioPluginServiceInformation>
    {
        val intent = Intent(AAP_ACTION_NAME)
        val resolveInfos = context.packageManager.queryIntentServices(intent, PackageManager.GET_META_DATA)
        var plugins = mutableListOf<AudioPluginServiceInformation>()
        for (ri in resolveInfos) {
            Log.i("AAP", "Service " + ri.serviceInfo.name)
            // TODO: it is super hacky so far.
            val xp = ri.serviceInfo.loadXmlMetaData(context.packageManager, AAP_ACTION_NAME)
            val aapServiceInfo = AudioPluginServiceInformation(ri.serviceInfo.loadLabel(context.packageManager).toString(), ri.serviceInfo.packageName, ri.serviceInfo.name)
            plugins.add(aapServiceInfo)

            var currentPlugin : PluginInformation? = null
            while (true) {
                var eventType = xp.next();
                if (eventType == XmlPullParser.END_DOCUMENT)
                    break
                if (eventType == XmlPullParser.IGNORABLE_WHITESPACE)
                    continue
                if (eventType == XmlPullParser.START_TAG) {
                    Log.i("AAPXML", "element: " + xp.name)
                    if(xp.name == "plugin") {
                        val name = xp.getAttributeValue(null, "name")
                        val backend = xp.getAttributeValue(null, "backend")
                        val category = xp.getAttributeValue(null, "category")
                        val author = xp.getAttributeValue(null, "author")
                        val manufacturer = xp.getAttributeValue(null, "manufacturer")
                        Log.i("AAPXML", "plugin name: " + name)
                        // isOutProcess is always true here - as it is returning "service" information here.
                        // Local plugin lookup may return the same plugins with in-process mode.
                        currentPlugin = PluginInformation(name, backend, category, author, manufacturer, true)
                        aapServiceInfo.plugins.add(currentPlugin)
                    } else if (xp.name == "port") {
                        if (currentPlugin != null) {
                            for (a in 0 .. xp.attributeCount - 1)
                                Log.i("AAPXML", "  port attribute: " + xp.getAttributeName(a))
                            val name = xp.getAttributeValue(null, "name")
                            val direction = xp.getAttributeValue(null, "direction")
                            val content = xp.getAttributeValue(null, "content")
                            val port = PortInformation(name,
                                if (direction == "input") PortInformation.PORT_DIRECTION_INPUT else PortInformation.PORT_DIRECTION_OUTPUT,
                                if (content == "midi") PortInformation.PORT_CONTENT_TYPE_MIDI else if (content == "audio") PortInformation.PORT_CONTENT_TYPE_AUDIO else PortInformation.PORT_CONTENT_TYPE_UNDEFINED)
                            currentPlugin.ports.add(port)
                        }
                    }
                }
                if (eventType == XmlPullParser.END_TAG) {
                    Log.i("AAPXML", "EndElement: " + xp.name)
                    if (xp.name == "plugin")
                        currentPlugin = null
                }
                else
                    Log.i("AAPXML", "node: " + eventType)
            }
            Log.i("AAP-Query","metadata: " + xp)
        }

        return plugins.toTypedArray()
    }
}