package org.androidaudioplugin.ui.androidx

import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.ArrayAdapter
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import org.androidaudioplugin.AudioPluginHostHelper
import org.androidaudioplugin.AudioPluginService
import org.androidaudioplugin.AudioPluginServiceInformation
import org.androidaudioplugin.PluginInformation
import org.androidaudioplugin.ui.androidx.databinding.AudioPluginServiceListItemBinding
import org.androidaudioplugin.ui.androidx.databinding.LocalPluginListBinding

class LocalPluginListActivity : AppCompatActivity() {

    inner class PluginViewAdapter(ctx:Context, layout: Int, array: Array<Pair<AudioPluginServiceInformation,PluginInformation>>)
        : ArrayAdapter<Pair<AudioPluginServiceInformation,PluginInformation>>(ctx, layout, array)
    {
        override fun getView(position: Int, convertView: View?, parent: ViewGroup): View {
            var binding =
                if (convertView != null) AudioPluginServiceListItemBinding.bind(convertView)
                else AudioPluginServiceListItemBinding.inflate(LayoutInflater.from(this@LocalPluginListActivity))
            val item = getItem(position)
            var view = binding.root
            if (item == null)
                throw UnsupportedOperationException("item not available!?")
            binding.audioPluginServiceName.text = item.first.label
            binding.audioPluginName.text = item.second.displayName
            binding.audioPluginIdentifier.text = item.second.pluginId

            view.setOnClickListener {
                var intent = Intent(context, LocalPluginDetailsActivity::class.java).apply {
                    putExtra("pluginId", item.second.pluginId)
                }
                startActivity(intent)
            }

            return view
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        val binding = LocalPluginListBinding.inflate(LayoutInflater.from(this))
        setContentView(binding.root)

        // Query AAPs
        val pluginServices = AudioPluginHostHelper.queryAudioPluginServices(this)
        val servicedPlugins = pluginServices.flatMap { s -> s.plugins.map { p -> Pair(s, p) } }.toTypedArray()

        val localPlugins = servicedPlugins.filter { p -> p.first.packageName == applicationInfo.packageName }.toTypedArray()
        val localAdapter = PluginViewAdapter(this, R.layout.audio_plugin_service_list_item, localPlugins)
        binding.localAudioPluginListView.adapter = localAdapter

        // It is a hidden clickable label to enable debugger
        this.findViewById<View>(R.id.localPluginsLabel).setOnClickListener {
            AudioPluginService.enableDebug = true
            Toast.makeText(this, "Debugging mode enabled: now the service will block until debugger gets connected",
                Toast.LENGTH_LONG).show()
        }

        // Show permissions dialog if any of them are required.
        var readExtStorage = android.Manifest.permission.READ_EXTERNAL_STORAGE
        if (packageManager.getPackageInfo(packageName, PackageManager.GET_PERMISSIONS)
                        .requestedPermissions.contains(readExtStorage) &&
                checkSelfPermission(readExtStorage) == PackageManager.PERMISSION_DENIED)
            requestPermissions(arrayOf(readExtStorage), 0)
    }
}
