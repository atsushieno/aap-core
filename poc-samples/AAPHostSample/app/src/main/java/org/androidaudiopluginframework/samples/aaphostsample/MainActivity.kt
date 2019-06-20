package org.androidaudiopluginframework.samples.aaphostsample

import android.content.ComponentName
import android.content.Context
import android.content.Intent
import android.content.ServiceConnection
import android.content.pm.PackageManager
import android.os.AsyncTask
import androidx.appcompat.app.AppCompatActivity
import android.os.Bundle
import android.os.IBinder
import android.util.Log
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.ArrayAdapter
import android.widget.CompoundButton
import android.widget.ListAdapter
import android.widget.TextView
import androidx.recyclerview.widget.RecyclerView
import org.androidaudiopluginframework.AudioPluginHost
import org.androidaudiopluginframework.AudioPluginService
import org.androidaudiopluginframework.hosting.AAPLV2Host
import org.androidaudiopluginframework.AudioPluginServiceInformation
import org.androidaudiopluginframework.PluginInformation
import kotlinx.android.synthetic.main.activity_main.*
import kotlinx.android.synthetic.main.audio_plugin_service_list_item.*
import kotlinx.android.synthetic.main.audio_plugin_service_list_item.view.*

class MainActivity : AppCompatActivity() {

    inner class PluginViewAdapter(ctx:Context, layout: Int, isOutProcess: Boolean, array: Array<Pair<AudioPluginServiceInformation,PluginInformation>>)
        : ArrayAdapter<Pair<AudioPluginServiceInformation,PluginInformation>>(ctx, layout, array)
    {
        var intent: Intent? = null
        val isOutProcess: Boolean = isOutProcess

        var conn = object : ServiceConnection {
            override fun onServiceConnected(name: ComponentName?, binder: IBinder?) {
                Log.i("HostMainActivity", "onServiceConnected invoked")
            }

            override fun onServiceDisconnected(name: ComponentName?) {
                Log.i("HostMainActivity", "onServiceDisconnected invoked")
            }
        }

        override fun getView(position: Int, convertView: View?, parent: ViewGroup): View {
            val item = getItem(position)
            var view = convertView
            if (view == null)
                view = LayoutInflater.from(context).inflate(R.layout.audio_plugin_service_list_item, parent, false)
            if (view == null)
                throw UnsupportedOperationException()
            if (item == null)
                return view
            view.audio_plugin_service_name.text = item.first.name
            view.audio_plugin_name.text = item.second.name
            view.audio_plugin_list_identifier.text = item.second.uniqueId

            val service = item.first
            val plugin = item.second

            if (isOutProcess) {
                view.plugin_toggle_switch.setOnCheckedChangeListener { compoundButton: CompoundButton, b: Boolean ->
                    if (intent != null) {
                        Log.i("Stopping ", "${service.name} | ${service.packageName} | ${service.className}")
                        context.stopService(intent)
                        context.unbindService(conn)
                        intent = null
                    } else {
                        Log.i("Instantiating ", "${service.name} | ${service.packageName} | ${service.className}")

                        intent = Intent(AudioPluginHost.AAP_ACTION_NAME)
                        intent!!.component = ComponentName(
                            service.packageName,
                            service.className
                        )

                        context.bindService(intent, conn, Context.BIND_AUTO_CREATE)
                        if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.O)
                            context.startForegroundService(intent)
                        else
                            context.startService(intent)
                    }
                }
            } else {
                view.plugin_toggle_switch.setOnCheckedChangeListener { compoundButton: CompoundButton, b: Boolean ->
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
                    val uri = item.second.uniqueId?.substring("lv2:".length)
                    AAPLV2Host.runHost(pluginPaths, this@MainActivity.assets, arrayOf(uri!!))
                }
            }

            return view
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        // Query AAPs
        val pluginServices = AudioPluginHost().queryAudioPluginServices(this)
        val servicedPlugins = pluginServices.flatMap { s -> s.plugins.map { p -> Pair(s, p) } }.toTypedArray()

        val servicedAdapter = PluginViewAdapter(this, R.layout.audio_plugin_service_list_item, true, servicedPlugins)
        this.audioPluginServiceListView.adapter = servicedAdapter

        val localPlugins = servicedPlugins.filter { p -> p.first.packageName == applicationInfo.packageName && p.second.backend == "LV2" }.toTypedArray()
        val localAdapter = PluginViewAdapter(this, R.layout.audio_plugin_service_list_item, false, localPlugins)
        this.localAudioPluginListView.adapter = localAdapter
    }
}
