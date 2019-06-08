package org.androidaudiopluginframework.samples.aapbarebonesample

import android.content.ComponentName
import android.content.Intent
import android.content.pm.PackageManager
import android.net.Uri
import androidx.appcompat.app.AppCompatActivity
import android.os.Bundle
import android.util.Log
import org.xmlpull.v1.XmlPullParser
import javax.xml.xpath.XPath

class MainActivity : AppCompatActivity() {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        // Query AAPs
        AndroidAudioPluginHost().queryAAPs(this)

        // query specific package
        var intent = Intent(AndroidAudioPluginService.AAP_ACTION_NAME)
        intent.component = ComponentName("org.androidaudiopluginframework.samples.aapbarebonesample",
            "org.androidaudiopluginframework.samples.aapbarebonesample.AndroidAudioPluginService")
        intent.type = "application/vnd.org.androidaudiopluginframework.control"
        Log.d("MainActivity", "calling startService")
        startService(intent)
        var intent2 = Intent(this, AndroidAudioPluginService::class.java)
        Log.d("MainActivityXXXXXXXXXXXXX", "intent2: " + intent2)
        //startService(intent2)
        Log.d("MainActivity", "startService done")

        stopService(intent)
        //stopService(intent2)
        Log.d("MainActivity", "stopService done")
    }
}
