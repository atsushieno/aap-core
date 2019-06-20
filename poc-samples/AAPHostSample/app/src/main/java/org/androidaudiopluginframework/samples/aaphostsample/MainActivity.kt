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
import org.androidaudiopluginframework.AudioPluginService
import org.androidaudiopluginframework.hosting.AAPLV2Host
import java.util.*
import kotlin.concurrent.timer

class MainActivity : AppCompatActivity() {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        // Query AAPs
        val plugins = AudioPluginHost().queryAudioPluginServices(this)
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

            //bindService(intent, conn, Context.BIND_AUTO_CREATE)
            if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.O)
                startForegroundService(intent)
            else
                startService(intent)

            //stopService(intent)
            //unbindService(conn)
        }

        // LV2 hosting
        //AAPLV2Host.runHost(arrayOf())
        var pluginPaths = arrayOf(
            "/lv2/presets.lv2/",
            "/lv2/eg-fifths.lv2/",
            "/lv2/dynmanifest.lv2/",
            "/lv2/port-groups.lv2/",
            "/lv2/urid.lv2/",
            "/lv2/morph.lv2/",
            "/lv2/eg-scope.lv2/",
            "/lv2/eg-amp.lv2/",
            "/lv2/eg-metro.lv2/",
            "/lv2/eg-sampler.lv2/",
            "/lv2/buf-size.lv2/",
            "/lv2/atom.lv2/",
            "/lv2/worker.lv2/",
            "/lv2/resize-port.lv2/",
            "/lv2/midi.lv2/",
            "/lv2/time.lv2/",
            "/lv2/units.lv2/",
            "/lv2/mda.lv2/",
            "/lv2/core.lv2/",
            "/lv2/log.lv2/",
            "/lv2/schemas.lv2/",
            "/lv2/eg-midigate.lv2/",
            "/lv2/eg-params.lv2/",
            "/lv2/state.lv2/",
            "/lv2/instance-access.lv2/",
            "/lv2/patch.lv2/",
            "/lv2/data-access.lv2/",
            "/lv2/options.lv2/",
            "/lv2/parameters.lv2/",
            "/lv2/event.lv2/",
            "/lv2/uri-map.lv2/",
            "/lv2/port-props.lv2/",
            "/lv2/ui.lv2/"
        )
        AAPLV2Host.runHost(pluginPaths, assets, arrayOf("http://drobilla.net/plugins/mda/Delay"))
        //AAPLV2Host.runHost(pluginPaths, assets, arrayOf("http://drobilla.net/plugins/mda/DX10"))
    }
}
