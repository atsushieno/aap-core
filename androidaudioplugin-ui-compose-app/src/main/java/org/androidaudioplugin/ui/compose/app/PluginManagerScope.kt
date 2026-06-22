package org.androidaudioplugin.ui.compose.app

import android.content.Context
import android.media.AudioManager
import android.os.Build
import android.util.Log
import java.io.InputStream
import java.io.OutputStream
import java.nio.ByteBuffer
import java.nio.ByteOrder
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.runtime.snapshots.SnapshotStateList
import androidx.compose.runtime.toMutableStateList
import org.androidaudioplugin.PluginInformation
import org.androidaudioplugin.PluginServiceInformation
import org.androidaudioplugin.composeaudiocontrols.DiatonicKeyboardNoteExpressionOrigin
import org.androidaudioplugin.hosting.AudioPluginClientBase
import org.androidaudioplugin.js.AapAutomationRuntime
import org.json.JSONArray
import org.json.JSONObject
import org.androidaudioplugin.hosting.AudioPluginMidiSettings
import org.androidaudioplugin.hosting.GuiHelper
import org.androidaudioplugin.hosting.NativeRemotePluginInstance
import org.androidaudioplugin.hosting.PluginServiceConnection
import org.androidaudioplugin.hosting.UmpHelper
import org.androidaudioplugin.manager.PluginPlayer
import kotlinx.coroutines.runBlocking

// Conventional default control buffer size (matches the host's DEFAULT_CONTROL_BUFFER_SIZE).
private const val DEFAULT_CONTROL_BYTES_PER_BLOCK = 0x10000

// Exposes the underlying native client pointer (aap::PluginClient*) so the embedded JS automation
// runtime can drive the very same client the Compose manager UI uses. `native` is protected on
// AudioPluginClientBase and NativePluginClient.native is the C++ handle.
private class AutomationCapableAudioPluginClient(context: Context) : AudioPluginClientBase(context) {
    val nativeClientHandle: Long get() = native.native
}

// Serializes the discovered plugin list for `aap.discovery.getPlugins()` (discovery is JVM-side).
private fun buildAutomationCatalogJson(services: List<PluginServiceInformation>): String {
    val array = JSONArray()
    services.forEach { service ->
        service.plugins.forEach { plugin ->
            array.put(JSONObject().apply {
                put("pluginId", plugin.pluginId)
                put("displayName", plugin.displayName)
                put("packageName", service.packageName)
            })
        }
    }
    return array.toString()
}

class PluginManagerScope(val context: Context,
                         val pluginServices: SnapshotStateList<PluginServiceInformation>
) : AutoCloseable {
    val logTag = "AAPPluginManager"

    private val automationClient = AutomationCapableAudioPluginClient(context).apply {
        onConnectedListeners.add { conn -> connections.add(conn) }
        onDisconnectingListeners.add { conn -> connections.remove(conn) }
    }
    val client: AudioPluginClientBase = automationClient

    // An observable list version of service connections
    val connections = listOf<PluginServiceConnection>().toMutableStateList()

    init {
        try {
            AapAutomationRuntime.bootstrap(context)
            AapAutomationRuntime.attachNativeClient(automationClient.nativeClientHandle)
            AapAutomationRuntime.setPluginCatalog(buildAutomationCatalogJson(pluginServices))
            // Let the JS facade bind plugin services before instancing (suspend -> blocking; this
            // runs on the runtime's executor thread, not main, so runBlocking is safe).
            AapAutomationRuntime.serviceConnector = { packageName ->
                runBlocking {
                    automationClient.connectToPluginService(packageName)
                    true
                }
            }
        } catch (e: Throwable) {
            Log.w(logTag, "Failed to initialize AAP JS automation runtime", e)
        }
    }

    override fun close() {
        client.dispose()
    }
}

class PluginDetailsScope(val pluginInfo: PluginInformation,
                         val manager: PluginManagerScope) : AutoCloseable {
    var instance = mutableStateOf<NativeRemotePluginInstance?>(null)
    var isProcessing = mutableStateOf(false)
    val parameterValues = mutableStateListOf<Double>()
    val outputMessages = mutableStateListOf<String>()
    private val midiOutputScratch = ByteArray(8192)
    private val parameterIdToIndex = mutableMapOf<Int, Int>()

    // Audio configuration shared between the eager prepare() at instantiation time and the
    // PluginPlayer created on first playback. They MUST be the same values: AudioPluginNode::start()
    // only calls prepare() while the instance is still UNPREPARED, so if we prepare early the player
    // reuses that preparation as-is. Computing both from the same source keeps the buffer layout
    // consistent.
    private val audioManager by lazy { manager.context.getSystemService(Context.AUDIO_SERVICE) as AudioManager }
    private val sampleRate by lazy { audioManager.getProperty(AudioManager.PROPERTY_OUTPUT_SAMPLE_RATE).toInt() }
    // It is for the audio processor's callback
    // FIXME: make them configurable?
    private val framesPerCallback by lazy { audioManager.getProperty(AudioManager.PROPERTY_OUTPUT_FRAMES_PER_BUFFER).toInt() }
    private val channelCount = 2

    private val pluginPlayerDelegate = lazy {
        PluginPlayer.create(sampleRate, framesPerCallback, channelCount).apply {
            setPlugin(instance.value!!)
            manager.context.assets.open(PluginPlayer.sample_audio_filename).use {
                val bytes = ByteArray(it.available())
                it.read(bytes)
                loadAudioResource(bytes, PluginPlayer.sample_audio_filename)
            }
        }
    }
    private val pluginPlayer by pluginPlayerDelegate

    private var alreadyDisposed = false

    override fun close() {
        if (alreadyDisposed)
            return
        alreadyDisposed = true
        if (pluginPlayerDelegate.isInitialized())
            pluginPlayer.close()
    }

    suspend fun instantiatePlugin() {
        if (!manager.connections.any { it.serviceInfo.packageName == pluginInfo.packageName })
            manager.client.connectToPluginService(pluginInfo.packageName)
        val result = manager.client.instantiateNativePlugin(pluginInfo)
        if (!alreadyDisposed) {
            // Prepare the instance up front so that preset/state/parameter interactions on
            // PluginDetails are valid before playback starts. Plugins (e.g. the LV2 bridge) may
            // touch process-time buffers when restoring a preset/state, and those only exist after
            // prepare(). The PluginPlayer later reuses this preparation since AudioPluginNode::start()
            // only prepares while UNPREPARED. The control buffer size is ignored by the host (it uses
            // its own DEFAULT_CONTROL_BUFFER_SIZE), but we pass the conventional value for clarity.
            result.prepare(framesPerCallback, sampleRate, DEFAULT_CONTROL_BYTES_PER_BLOCK)
            instance.value = result
            syncParametersFromInstance()
        }
    }

    fun setNewMidiMappingFlags(pluginId: String, newFlags: Int) {
        AudioPluginMidiSettings.putMidiSettingsToSharedPreference(
            manager.context,
            pluginId,
            newFlags
        )
    }

    fun enableAudioRecorder() {
        pluginPlayer.enableAudioRecorder()
    }

    fun startProcessing() {
        if (alreadyDisposed)
            return
        pluginPlayer.startProcessing()
        isProcessing.value = true
    }

    fun pauseProcessing() {
        if (pluginPlayerDelegate.isInitialized())
            pluginPlayer.pauseProcessing()
        isProcessing.value = false
    }

    fun playPreloadedAudio() {
        pluginPlayer.playPreloadedAudio()
    }

    fun setPresetIndex(index: Int) {
        if (isProcessing.value)
            pluginPlayer.setPresetIndex(index)
        else
            instance.value?.setCurrentPresetIndex(index)
        syncParametersFromInstance()
    }

    fun setParameterValue(index: Int, value: Float) {
        val ins = instance.value
        if (ins != null) {
            if (index in parameterValues.indices)
                parameterValues[index] = value.toDouble()
            pluginPlayer.setParameterValue(ins.getParameter(index), value.toDouble())
        }
    }

    fun processExpression(origin: DiatonicKeyboardNoteExpressionOrigin, note: Int, value: Float) {
        when(origin) {
            DiatonicKeyboardNoteExpressionOrigin.HorizontalDragging -> pluginPlayer.processPitchBend(-1, value)
            DiatonicKeyboardNoteExpressionOrigin.VerticalDragging -> pluginPlayer.processPitchBend(note, value)
            DiatonicKeyboardNoteExpressionOrigin.Pressure -> pluginPlayer.processPressure(note, value)
        }
    }

    fun setNoteState(note: Int, isNoteOn: Boolean) {
        pluginPlayer.setNoteState(note, 0xF800, isNoteOn)
    }

    fun saveStateTo(output: OutputStream) {
        val ins = instance.value ?: return
        val size = ins.getStateSize()
        if (size <= 0)
            return
        val data = ByteArray(size)
        ins.getState(data)
        output.write(data)
        output.flush()
    }

    fun loadStateFrom(input: InputStream) {
        val ins = instance.value ?: return
        val data = input.readBytes()
        if (data.isNotEmpty())
            ins.setState(data)
        syncParametersFromInstance()
    }

    fun syncParametersFromInstance() {
        val ins = instance.value ?: return

        val count = ins.getParameterCount()
        val values = ArrayList<Double>(count)
        parameterIdToIndex.clear()
        for (index in 0 until count) {
            val parameter = ins.getParameter(index)
            parameterIdToIndex[parameter.id] = index
            values += ins.getParameterValue(index)
        }
        if (parameterValues.size != values.size) {
            parameterValues.clear()
            parameterValues.addAll(values)
        } else {
            values.forEachIndexed { index, value ->
                if (parameterValues[index] != value)
                    parameterValues[index] = value
            }
        }
    }

    fun drainMidiOutput() {
        syncParametersFromInstance()
        if (!isProcessing.value || !pluginPlayerDelegate.isInitialized())
            return
        val bytesRead = pluginPlayer.readMidiOutput(midiOutputScratch)
        if (bytesRead <= 0)
            return
        val buffer = ByteBuffer.wrap(midiOutputScratch, 0, bytesRead).order(ByteOrder.nativeOrder())
        while (buffer.remaining() >= Int.SIZE_BYTES) {
            val offset = buffer.position()
            val word0 = buffer.int
            val messageType: Int = (word0 ushr 28) and 0xF
            when (messageType) {
                4 -> {
                    if (buffer.remaining() < Int.SIZE_BYTES) {
                        buffer.position(bytesRead)
                        continue
                    }
                    val word1 = buffer.int
                    val group: Int = (word0 ushr 24) and 0xF
                    val status: Int = (word0 ushr 16) and 0xF0
                    val channel: Int = (word0 ushr 16) and 0xF
                    when (status) {
                        0x30 -> {
                            val parameterId = ((word0 ushr 8) and 0x7F) shl 7 or (word0 and 0x7F)
                            val value = parameterIdToIndex[parameterId]?.let { index ->
                                instance.value!!.getParameter(index).let { parameter ->
                                    UmpHelper.transportUint32ToPlain(parameter.minimumValue, parameter.maximumValue, word1)
                                }
                            } ?: 0.0
                            if (channel == 0)
                                parameterIdToIndex[parameterId]?.let { index ->
                                    if (index in parameterValues.indices)
                                        parameterValues[index] = value
                                }
                            appendOutputMessage("G$group ch$channel NRPN $parameterId = $value")
                        }
                        0xB0 -> {
                            val parameterId = (word0 ushr 8) and 0x7F
                            val value = parameterIdToIndex[parameterId]?.let { index ->
                                instance.value!!.getParameter(index).let { parameter ->
                                    UmpHelper.transportUint32ToPlain(parameter.minimumValue, parameter.maximumValue, word1)
                                }
                            } ?: 0.0
                            if (channel == 0)
                                parameterIdToIndex[parameterId]?.let { index ->
                                    if (index in parameterValues.indices)
                                        parameterValues[index] = value
                                }
                            appendOutputMessage("G$group ch$channel CC $parameterId = $value")
                        }
                        else -> appendOutputMessage("G$group ch$channel status 0x${status.toString(16)}")
                    }
                }
                5 -> {
                    if (buffer.remaining() < Int.SIZE_BYTES * 3) {
                        buffer.position(bytesRead)
                        continue
                    }
                    val word1 = buffer.int
                    val word2 = buffer.int
                    val word3 = buffer.int
                    val group: Int = (word0 ushr 24) and 0xF
                    val channel: Int = word1 and 0xF
                    if ((word0 and 0xFF) == 0x7E && word1 ushr 8 == 0x7F0000) {
                        val parameterId = word2 and 0xFFFF
                        val value = parameterIdToIndex[parameterId]?.let { index ->
                            instance.value!!.getParameter(index).let { parameter ->
                                UmpHelper.transportUint32ToPlain(parameter.minimumValue, parameter.maximumValue, word3)
                            }
                        } ?: 0.0
                        if (channel == 0)
                            parameterIdToIndex[parameterId]?.let { index ->
                                if (index in parameterValues.indices)
                                    parameterValues[index] = value
                            }
                        appendOutputMessage("G$group ch$channel SysEx8 $parameterId = $value")
                    } else {
                        appendOutputMessage("G$group SysEx8 message")
                    }
                }
                else -> {
                    val messageSize = when (messageType) {
                        0, 1, 2, 6, 7 -> Int.SIZE_BYTES
                        3, 4, 8, 9, 0xA -> Int.SIZE_BYTES * 2
                        0xB, 0xC -> Int.SIZE_BYTES * 3
                        else -> Int.SIZE_BYTES * 4
                    }
                    buffer.position((offset + messageSize).coerceAtMost(bytesRead))
                }
            }
        }
    }

    private fun appendOutputMessage(message: String) {
        outputMessages.add(0, message)
        while (outputMessages.size > 16)
            outputMessages.removeAt(outputMessages.lastIndex)
    }
 }

class SurfaceControlUIScope private constructor(
    private val parentScope: PluginDetailsScope,
    val width: Int,
    val height: Int
) : AutoCloseable {
    companion object {
        suspend fun create(parentScope: PluginDetailsScope, fallbackWidth: Int, fallbackHeight: Int): SurfaceControlUIScope {
            val pluginInfo = parentScope.pluginInfo
            val instanceId = parentScope.instance.value!!.instanceId
            val guiHost = GuiHelper.NativeEmbeddedSurfaceControlHost(
                parentScope.manager.context,
                pluginInfo.packageName,
                pluginInfo.pluginId!!,
                instanceId
            )
            val preferredSize =
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R)
                    guiHost.getPreferredSizeOrFallback(fallbackWidth, fallbackHeight)
                else
                    GuiHelper.Size(fallbackWidth, fallbackHeight)
            val width = preferredSize.width
            val height = preferredSize.height
            val scope = SurfaceControlUIScope(parentScope, width, height)
            scope.guiHost = guiHost
            return scope
        }
    }

    var guiHost: GuiHelper.NativeEmbeddedSurfaceControlHost? = null

    suspend fun connectSurfaceControlUI() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            guiHost!!.connect(width, height)
        }
    }

    fun showSurfaceGUI() {
        guiHost?.show()
    }

    fun hideSurfaceGUI() {
        guiHost?.hide()
    }

    fun resizeSurfaceGUI(width: Int, height: Int) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            guiHost?.resize(width, height)
        }
    }

    fun configureSurfaceGUIViewport(viewportWidth: Int, viewportHeight: Int,
                                    contentWidth: Int, contentHeight: Int,
                                    scrollX: Int, scrollY: Int) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            guiHost?.configureViewport(
                GuiHelper.ViewportConfiguration(
                    viewportWidth,
                    viewportHeight,
                    contentWidth,
                    contentHeight,
                    scrollX,
                    scrollY
                )
            )
        }
    }

    override fun close() {
        guiHost?.close()
    }
}
