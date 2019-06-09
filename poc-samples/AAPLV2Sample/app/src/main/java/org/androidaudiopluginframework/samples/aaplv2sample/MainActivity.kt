package org.androidaudiopluginframework.samples.aaplv2sample

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
    }
}
