package org.androidaudiopluginframework.samples.aaphostsample

import android.content.ComponentName
import android.content.Context
import android.content.Intent
import android.content.ServiceConnection
import android.os.AsyncTask
import androidx.appcompat.app.AppCompatActivity
import android.os.Bundle
import android.os.IBinder
import android.util.Log
import org.androidaudiopluginframework.AudioPluginHost
import org.androidaudiopluginframework.hosting.AAPLV2Host
import java.util.*
import kotlin.concurrent.timer

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
            startForegroundService(intent)

            //timer(period = 2000, action = {
                stopService(intent)
                unbindService(conn)
            //})

        }

        // LV2 hosting
        //AAPLV2Host.runHost(arrayOf())
        var pluginPaths = arrayOf(
            "/lv2/x86_64/presets.lv2/",
            "/lv2/x86_64/eg-fifths.lv2/",
            "/lv2/x86_64/dynmanifest.lv2/",
            "/lv2/x86_64/port-groups.lv2/",
            "/lv2/x86_64/urid.lv2/",
            "/lv2/x86_64/morph.lv2/",
            "/lv2/x86_64/eg-scope.lv2/",
            "/lv2/x86_64/eg-amp.lv2/",
            "/lv2/x86_64/eg-metro.lv2/",
            "/lv2/x86_64/eg-sampler.lv2/",
            "/lv2/x86_64/buf-size.lv2/",
            "/lv2/x86_64/atom.lv2/",
            "/lv2/x86_64/worker.lv2/",
            "/lv2/x86_64/resize-port.lv2/",
            "/lv2/x86_64/midi.lv2/",
            "/lv2/x86_64/time.lv2/",
            "/lv2/x86_64/units.lv2/",
            "/lv2/x86_64/mda.lv2/",
            "/lv2/x86_64/core.lv2/",
            "/lv2/x86_64/log.lv2/",
            "/lv2/x86_64/schemas.lv2/",
            "/lv2/x86_64/eg-midigate.lv2/",
            "/lv2/x86_64/eg-params.lv2/",
            "/lv2/x86_64/state.lv2/",
            "/lv2/x86_64/instance-access.lv2/",
            "/lv2/x86_64/patch.lv2/",
            "/lv2/x86_64/data-access.lv2/",
            "/lv2/x86_64/options.lv2/",
            "/lv2/x86_64/parameters.lv2/",
            "/lv2/x86_64/event.lv2/",
            "/lv2/x86_64/uri-map.lv2/",
            "/lv2/x86_64/port-props.lv2/",
            "/lv2/x86_64/ui.lv2/"
        )
        //AAPLV2Host.runHost(pluginPaths, assets, arrayOf("http://drobilla.net/plugins/mda/Delay"))
        AAPLV2Host.runHost(pluginPaths, assets, arrayOf("http://drobilla.net/plugins/mda/DX10"))
    }
}
