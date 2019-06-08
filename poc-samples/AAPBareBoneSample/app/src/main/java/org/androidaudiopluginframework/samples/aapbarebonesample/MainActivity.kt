package org.androidaudiopluginframework.samples.aapbarebonesample

import android.content.ComponentName
import android.content.Intent
import android.content.pm.PackageManager
import android.net.Uri
import androidx.appcompat.app.AppCompatActivity
import android.os.Bundle
import android.util.Log
import org.androidaudiopluginframework.AudioPluginService
import org.xmlpull.v1.XmlPullParser
import javax.xml.xpath.XPath

class MainActivity : AppCompatActivity() {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        // Query AAPs
        val plugins = AndroidAudioPluginHost().queryAudioPlugins(this)
        for (plugin in plugins)
            Log.i("AAP-QueryResults", plugin)

        // query specific package
        var intent = Intent(AudioPluginService.AAP_ACTION_NAME)
        intent.component = ComponentName("org.androidaudiopluginframework.samples.aapbarebonesample",
            "org.androidaudiopluginframework.samples.aapbarebonesample.AAPBareBoneSampleService")
        startService(intent)
        stopService(intent)
    }
}
