package org.androidaudiopluginframework

import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.util.Log
import org.xmlpull.v1.XmlPullParser

class AudioPluginHost
{
    companion object {
        const val AAP_ACTION_NAME = "org.androidaudiopluginframework.AndroidAudioPluginService"
    }

    fun queryAudioPlugins(context: Context) : Array<PluginInformation>
    {
        val intent = Intent(AAP_ACTION_NAME)
        val resolveInfos = context.packageManager.queryIntentServices(intent, PackageManager.GET_META_DATA)
        var plugins = mutableListOf<PluginInformation>()
        for (ri in resolveInfos) {
            // TODO: it is super hacky so far.
            val xp = ri.serviceInfo.loadXmlMetaData(context.packageManager, AAP_ACTION_NAME)
            while (true) {
                var eventType = xp.next();
                if (eventType == XmlPullParser.END_DOCUMENT)
                    break
                if (eventType == XmlPullParser.IGNORABLE_WHITESPACE)
                    continue
                if (eventType == XmlPullParser.START_TAG) {
                    Log.i("AAPXML", "element: " + xp.name)
                    if(xp.name == "plugin") {
                        val name = xp.getAttributeValue(null, "product")
                        plugins.add(PluginInformation(name, ri.serviceInfo.packageName, ri.serviceInfo.name))
                    }
                }
                if (eventType == XmlPullParser.END_TAG)
                    Log.i("AAPXML", "EndElement: " + xp.name)
                else
                    Log.i("AAPXML", "node: " + eventType)
            }
            Log.i("AAP-Query","metadata: " + xp)
        }

        return plugins.toTypedArray()
    }
}