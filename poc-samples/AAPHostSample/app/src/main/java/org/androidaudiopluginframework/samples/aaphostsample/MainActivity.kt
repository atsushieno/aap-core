package org.androidaudiopluginframework.samples.aaphostsample

import android.content.ComponentName
import android.content.Context
import android.content.Intent
import android.content.ServiceConnection
import androidx.appcompat.app.AppCompatActivity
import android.os.Bundle
import android.os.IBinder
import android.util.Log
import org.androidaudiopluginframework.AudioPluginHost

class MainActivity : AppCompatActivity() {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        // Query AAPs
        val plugins = AudioPluginHost().queryAudioPlugins(this)
        Log.i("AAPPluginHostSample","Plugin query results:")
        for (plugin in plugins) {
            Log.i("Instantiating ", "${plugin.name} | ${plugin.packageName} | ${plugin.className}")

            // query specific package
            var intent = Intent(AudioPluginHost.AAP_ACTION_NAME)
            intent.component = ComponentName(
                plugin.packageName,
                plugin.className
            )

            var conn = object: ServiceConnection {
                override fun onServiceConnected(name: ComponentName?, binder: IBinder?) {
                    Log.i("HostMainActivity", "onServiceConnected invoked")
                }
                override fun onServiceDisconnected(name: ComponentName?) {
                    Log.i("HostMainActivity", "onServiceDisconnected invoked")
                }
            }

            bindService(intent, conn, Context.BIND_AUTO_CREATE)
            startService(intent)
            stopService(intent)
        }
    }
}
