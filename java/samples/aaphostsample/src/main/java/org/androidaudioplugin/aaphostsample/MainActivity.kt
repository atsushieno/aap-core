package org.androidaudioplugin.aaphostsample

import android.content.Context
import androidx.appcompat.app.AppCompatActivity
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.view.Window
import android.widget.ArrayAdapter
import android.widget.SeekBar
import android.widget.Toast
import kotlinx.coroutines.GlobalScope
import kotlinx.coroutines.launch
import org.androidaudioplugin.*
import org.androidaudioplugin.aaphostsample.databinding.ActivityMainBinding
import org.androidaudioplugin.aaphostsample.databinding.AudioPluginParametersListItemBinding
import org.androidaudioplugin.aaphostsample.databinding.AudioPluginServiceListItemBinding
import org.androidaudioplugin.samples.host.engine.PluginPreview
import kotlin.time.ExperimentalTime

@ExperimentalUnsignedTypes
class MainActivity : AppCompatActivity() {

    inner class PluginViewAdapter(ctx:Context, layout: Int, array: Array<Pair<AudioPluginServiceInformation,PluginInformation>>)
        : ArrayAdapter<Pair<AudioPluginServiceInformation,PluginInformation>>(ctx, layout, array)
    {
        override fun getView(position: Int, convertView: View?, parent: ViewGroup): View {
            val binding =
                if (convertView != null)
                    AudioPluginServiceListItemBinding.bind(convertView)
                else AudioPluginServiceListItemBinding.inflate(LayoutInflater.from(context))
            val item = getItem(position)
            val view = binding.root
            if (item == null)
                throw UnsupportedOperationException()

            val plugin = item.second

            if (plugin.pluginId == null)
                throw UnsupportedOperationException ("Insufficient plugin information was returned; missing pluginId")

            binding.audioPluginServiceName.text = item.first.label
            binding.audioPluginName.text = item.second.displayName
            binding.audioPluginListIdentifier.text = item.second.pluginId

            view.setOnClickListener {
                portsAdapter = PortViewAdapter(this@MainActivity, R.layout.audio_plugin_parameters_list_item, plugin.ports)
                this@MainActivity.selectedPluginIndex = position
                this@MainActivity.binding.audioPluginParametersListView.adapter = portsAdapter

                this@MainActivity.binding.buttonLaunchUi.setOnClickListener {
                    launchInProcessPluginUI(item.second)
                }
            }

            return view
        }
    }

    fun launchInProcessPluginUI(pluginInfo: PluginInformation) {
        val instanceId = if (preview.instance?.pluginInfo?.pluginId == pluginInfo.pluginId) preview.instance?.instanceId else null
        AudioPluginHostHelper.launchInProcessPluginUI(this, pluginInfo, instanceId)
    }

    inner class PortViewAdapter(ctx:Context, layout: Int, private var ports: List<PortInformation>)
        : ArrayAdapter<PortInformation>(ctx, layout, ports)
    {
        override fun getView(position: Int, convertView: View?, parent: ViewGroup): View
        {
            val item = getItem(position)
            val binding =
                if (convertView != null)
                    AudioPluginParametersListItemBinding.bind(convertView)
                else AudioPluginParametersListItemBinding.inflate(LayoutInflater.from(context))
            if (item == null)
                throw UnsupportedOperationException()

            binding.audioPluginParameterContentType.text = if(item.content == 1) "Audio" else if(item.content == 2) "Midi" else "Other"
            binding.audioPluginParameterDirection.text = if(item.direction == 0) "In" else "Out"
            binding.audioPluginParameterName.text = item.name
            binding.audioPluginSeekbarParameterValue.setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
                override fun onProgressChanged(seekBar: SeekBar?, progress: Int, fromUser: Boolean) {
                    if (!fromUser)
                        return
                    parameters[position] = progress / 100.0f
                    binding.audioPluginEditTextParameterValue.text.clear()
                    binding.audioPluginEditTextParameterValue.text.insert(0, parameters[position].toString())
                }
                override fun onStartTrackingTouch(seekBar: SeekBar?) {
                }

                override fun onStopTrackingTouch(seekBar: SeekBar?) {
                }
            })
            val value = parameters[ports.indexOf(item)]
            binding.audioPluginSeekbarParameterValue.progress = (100.0 * value).toInt()

            return binding.root
        }

        val parameters = ports.map { p -> p.default }.toFloatArray()
    }

    private lateinit var binding : ActivityMainBinding

    private lateinit var preview : PluginPreview
    private var portsAdapter : PortViewAdapter? = null
    private var selectedPluginIndex : Int = -1

    override fun onDestroy() {
        super.onDestroy()
        preview.dispose()
    }

    @ExperimentalTime
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        requestWindowFeature(Window.FEATURE_NO_TITLE)
        binding = ActivityMainBinding.inflate(LayoutInflater.from(this))
        setContentView(binding.root)

        preview = PluginPreview(this)
        preview.processAudioCompleted = { onProcessAudioCompleted() }

        Toast.makeText(this, "loaded input wav", Toast.LENGTH_LONG).show()

        binding.wavePostPlugin.sample = arrayOf(0, 0, 0, 0).toIntArray() // dummy

        // Query AAPs
        val pluginServices = AudioPluginHostHelper.queryAudioPluginServices(this)
        val servicedPlugins = pluginServices.flatMap { s -> s.plugins.map { p -> Pair(s, p) } }.toTypedArray()

        val plugins = servicedPlugins.filter { p -> p.first.packageName != applicationInfo.packageName }.toTypedArray()
        val adapter = PluginViewAdapter(this, R.layout.audio_plugin_service_list_item, plugins)
        binding.audioPluginListView.adapter = adapter


        binding.applyToggleButton.setOnCheckedChangeListener { _, checked ->
            if (checked) {
                if (selectedPluginIndex < 0)
                    return@setOnCheckedChangeListener
                var item = adapter.getItem(selectedPluginIndex)
                if (item != null)
                    preview.applyPlugin(item.first, item.second, portsAdapter?.parameters)
            } else {
                binding.wavePostPlugin.sample = preview.inBuf.map { b -> b.toInt() }.toIntArray()
                binding.wavePostPlugin.requestLayout()
            }
        }
        binding.playPostPluginLabel.setOnClickListener { GlobalScope.launch {preview.playSound(binding.applyToggleButton.isChecked) } }

        binding.wavePostPlugin.sample = preview.inBuf.map { b -> b.toInt() }.toIntArray()
        binding.wavePostPlugin.requestLayout()
    }

    private fun onProcessAudioCompleted()
    {
        runOnUiThread {
            binding.wavePostPlugin.sample = preview.outBuf.map { b -> b.toInt() }.toIntArray()
            Toast.makeText(this@MainActivity, "set output wav", Toast.LENGTH_LONG).show()
        }
        preview.unbindHost()
    }
}
