package org.androidaudiopluginframework.samples.aapbarebonesample

import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.util.Log
import org.androidaudiopluginframework.AudioPluginService
import org.xmlpull.v1.XmlPullParser

class AndroidAudioPluginHost
{
    fun queryAudioPlugins(context: Context) : Array<String>
    {
        val intent = Intent(AudioPluginService.AAP_ACTION_NAME)
        val resolveInfos = context.packageManager.queryIntentServices(intent, PackageManager.GET_META_DATA)
        var plugins = mutableListOf<String>()
        for (ri in resolveInfos) {
            val xp = ri.serviceInfo.loadXmlMetaData(context.packageManager, AudioPluginService.AAP_ACTION_NAME)
            while (true) {
                var eventType = xp.next();
                if (eventType == XmlPullParser.END_DOCUMENT)
                    break
                if (eventType == XmlPullParser.IGNORABLE_WHITESPACE)
                    continue
                if (eventType == XmlPullParser.START_TAG) {
                    Log.i("AAPXML", "element: " + xp.name)
                    if(xp.name == "plugin") {
                        plugins.add(ri.serviceInfo.name + "/" + xp.getAttributeValue(null, "product"))
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