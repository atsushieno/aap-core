package org.androidaudioplugin

import android.content.ComponentName
import android.content.Context
import android.content.Intent
import android.content.ServiceConnection
import android.os.IBinder
import android.util.Log
import java.lang.UnsupportedOperationException

// This class is not to endorse any official API for hosting AAP.
// Its implementation is hacky and not really with decent API design.
// It is to provide usable utilities for plugin developers as a proof of concept.
class AudioPluginHost(private var applicationContext: Context) {

    // Service connection

    var serviceConnectedListeners = mutableListOf<(conn: AudioPluginServiceConnection) -> Unit>()
    var pluginInstantiatedListeners = mutableListOf<(conn: AudioPluginInstance) -> Unit>()

    var connectedServices = mutableListOf<AudioPluginServiceConnection>()
    var instantiatedPlugins = mutableListOf<AudioPluginInstance>()

    fun bindAudioPluginService(service: AudioPluginServiceInformation) {
        var intent = Intent(AudioPluginHostHelper.AAP_ACTION_NAME)
        intent.component = ComponentName(
            service.packageName,
            service.className
        )
        intent.putExtra("sampleRate", this.sampleRate)

        var conn = AudioPluginServiceConnection(service, { c -> onBindAudioPluginService(c) })

        Log.d("AudioPluginHost", "bindAudioPluginService: ${service.packageName} | ${service.className}")
        applicationContext.bindService(intent, conn, Context.BIND_AUTO_CREATE)
    }

    private fun onBindAudioPluginService(conn: AudioPluginServiceConnection) {
        var serviceIdentifier = "${conn.serviceInfo.packageName}/${conn.serviceInfo.className}"
        AudioPluginNatives.addBinderForHost(serviceIdentifier, conn.binder!!)
        connectedServices.add(conn)
        serviceConnectedListeners.forEach { f -> f(conn) }
    }

    private fun findExistingServiceConnection(serviceIdentifier: String) : AudioPluginServiceConnection? {
        var idx = serviceIdentifier.indexOf('/')
        var packageName = serviceIdentifier.substring(0, idx)
        var className = serviceIdentifier.substring(idx + 1)
        var svc = connectedServices.firstOrNull { conn -> conn.serviceInfo.packageName == packageName && conn.serviceInfo.className == className }
        return svc
    }

    fun unbindAudioPluginService(serviceIdentifier: String) {
        var conn = findExistingServiceConnection(serviceIdentifier)
        if (conn == null)
            return
        connectedServices.remove(conn)
        AudioPluginNatives.removeBinderForHost(serviceIdentifier)
    }

    // Plugin instancing

    fun instantiatePlugin(pluginInfo: PluginInformation)
    {
        var conn = findExistingServiceConnection(pluginInfo.serviceIdentifier)
        if (conn == null) {
            var serviceConnectedListener =
                { conn: AudioPluginServiceConnection -> instantiatePlugin(pluginInfo, conn) }
            var pluginInstantiatedListener: (AudioPluginInstance) -> Unit = { plugin -> {} }
            pluginInstantiatedListener = { plugin ->
                pluginInstantiatedListeners.remove(pluginInstantiatedListener)
                serviceConnectedListeners.remove(serviceConnectedListener)
            }
            serviceConnectedListeners.add(serviceConnectedListener)
            var service = AudioPluginServiceInformation("", pluginInfo.serviceIdentifier)
            bindAudioPluginService(service)
        }
        else
            instantiatePlugin(pluginInfo, conn)
    }

    private fun instantiatePlugin(pluginInfo: PluginInformation, conn: AudioPluginServiceConnection)
    {
        var instance = AudioPluginInstance(pluginInfo, conn)
        instantiatedPlugins.add(instance)
        pluginInstantiatedListeners.forEach { l -> l (instance) }
    }

    // Audio buses and buffers management

    var audioInputs = mutableListOf<ByteArray>()
    var controlInputs = mutableListOf<ByteArray>()
    var audioOutputs = mutableListOf<ByteArray>()

    var isConfigured = false
    var isActive = false

    var sampleRate = 44100

    var audioBufferSizeInBytes = 44100 * 4
        set(value) {
            field = value
            resetInputBuffer()
            resetOutputBuffer()
        }
    var controlBufferSizeInBytes = 44100 * 4
        set(value) {
            field = value
            resetControlBuffer()
        }

    fun throwIfRunning() { if (isActive) throw UnsupportedOperationException("AudioPluginHost is already running") }

    var inputAudioBus = AudioBusPresets.stereo // it will be initialized at init() too for allocating buffers.
        set(value) {
            throwIfRunning()
            field = value
            resetInputBuffer()
        }
    fun resetInputBuffer() = expandBufferArrays(audioInputs, inputAudioBus.map.size, audioBufferSizeInBytes)

    var inputControlBus = AudioBusPresets.monoral
        set(value) {
            throwIfRunning()
            field = value
            resetControlBuffer()
        }
    fun resetControlBuffer() = expandBufferArrays(controlInputs, inputControlBus.map.size, controlBufferSizeInBytes)

    var outputAudioBus = AudioBusPresets.stereo // it will be initialized at init() too for allocating buffers.
        set(value) {
            throwIfRunning()
            field = value
            resetOutputBuffer()
        }
    fun resetOutputBuffer() = expandBufferArrays(audioOutputs, outputAudioBus.map.size, audioBufferSizeInBytes)

    fun expandBufferArrays(list : MutableList<ByteArray>, newSize : Int, bufferSize : Int) {
        if (newSize > list.size)
            (0 until newSize - list.size).forEach {list.add(ByteArray(bufferSize)) }
        while (newSize < list.size)
            list.remove(list.last())
        for (i in 0 until newSize)
            if (list[i].size < bufferSize)
                list[i] = ByteArray(bufferSize)
    }

    init {
        AudioPluginNatives.setApplicationContext(applicationContext)
        AudioPluginNatives.initialize(AudioPluginHostHelper.queryAudioPluginServices(applicationContext).flatMap { s -> s.plugins }.toTypedArray())
        inputAudioBus = AudioBusPresets.stereo
        outputAudioBus = AudioBusPresets.stereo
    }
}

class AudioBus(var name : String, var map : Map<String,Int>)

class AudioBusPresets
{
    companion object {
        val monoral = AudioBus("Monoral", mapOf(Pair("center", 0)))
        val stereo = AudioBus("Stereo", mapOf(Pair("Left", 0), Pair("Right", 1)))
        val surround50 = AudioBus(
            "5.0 Surrounded", mapOf(
                Pair("Left", 0), Pair("Center", 1), Pair("Right", 2),
                Pair("RearLeft", 3), Pair("RearRight", 4)
            )
        )
        val surround51 = AudioBus(
            "5.1 Surrounded", mapOf(
                Pair("Left", 0), Pair("Center", 1), Pair("Right", 2),
                Pair("RearLeft", 3), Pair("RearRight", 4), Pair("LowFrequencyEffect", 5)
            )
        )
        val surround61 = AudioBus(
            "6.1 Surrounded", mapOf(
                Pair("Left", 0), Pair("Center", 1), Pair("Right", 2),
                Pair("SideLeft", 3), Pair("SideRight", 4),
                Pair("RearCenter", 5), Pair("LowFrequencyEffect", 6)
            )
        )
        val surround71 = AudioBus(
            "7.1 Surrounded", mapOf(
                Pair("Left", 0), Pair("Center", 1), Pair("Right", 2),
                Pair("SideLeft", 3), Pair("SideRight", 4),
                Pair("RearLeft", 5), Pair("RearRight", 6), Pair("LowFrequencyEffect", 7)
            )
        )
    }
}

class AudioPluginServiceConnection(var serviceInfo: AudioPluginServiceInformation, var onConnectedCallback: (conn: AudioPluginServiceConnection) -> Unit) : ServiceConnection {

    var binder: IBinder? = null

    override fun onServiceConnected(name: ComponentName?, binder: IBinder?) {
        Log.d("AudioPluginServiceConnection", "onServiceConnected")
        this.binder = binder
        onConnectedCallback(this)
    }

    override fun onServiceDisconnected(name: ComponentName?) {
    }
}

class AudioPluginInstance(var pluginInfo: PluginInformation, var service: AudioPluginServiceConnection)
{
    init {
        AudioPluginNatives.instantiatePlugin(pluginInfo.serviceIdentifier, pluginInfo.pluginId!!)
    }
}
