package org.androidaudioplugin.ui.compose.app

import android.content.Context
import android.media.AudioManager
import android.os.Build
import android.os.Handler
import android.os.Looper
import android.util.Log
import java.io.InputStream
import java.io.OutputStream
import java.nio.ByteBuffer
import java.nio.ByteOrder
import androidx.compose.runtime.mutableIntStateOf
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
import org.androidaudioplugin.soundpack.SoundpackKind
import org.androidaudioplugin.soundpack.client.SoundpackClient
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.flow.flowOn
import kotlinx.coroutines.flow.map
import kotlinx.coroutines.launch
import kotlinx.coroutines.runBlocking
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock
import kotlinx.coroutines.withContext

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
    // Incremented whenever the plugin changes its parameter layout (names, ranges, and/or count),
    // so that views that render parameter metadata recompose even if the count did not change.
    val parameterLayoutRevision = mutableIntStateOf(0)
    val outputMessages = mutableStateListOf<String>()
    private val mainHandler = Handler(Looper.getMainLooper())
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

    /** What "Play Audio" plays: once the player exists, the source it has loaded. */
    val audioSource = mutableStateOf(AudioSourcePreferences.load(manager.context))
    val isLoadingAudioSource = mutableStateOf(false)
    val audioSourceError = mutableStateOf<String?>(null)
    /** What the user can choose from: the bundled sample, then every "AAP Sample" entry. Null
     * until [collectAudioSourceCandidates] has read the providers, and while the device has no
     * "AAP Sample" provider at all (no Soundpack Manager), so there is nothing to choose. */
    val audioSourceCandidates = mutableStateOf<List<AudioSource>?>(null)
    // Loads run one at a time and in order, so the last selection wins.
    private val audioSourceMutex = Mutex()
    private var pendingAudioSourceLoads = 0
    private var playerHasAudio = false
    private var playAfterLoading = false

    // Audio sources load in coroutines, which close() cancels.
    private val coroutineJob = SupervisorJob()
    private val coroutineScope = CoroutineScope(coroutineJob + Dispatchers.Main.immediate)

    private val pluginPlayerDelegate = lazy {
        PluginPlayer.create(sampleRate, framesPerCallback, channelCount).apply {
            setPlugin(instance.value!!)
        }.also { loadAudioSource(it, audioSource.value) }
    }
    private val pluginPlayer by pluginPlayerDelegate

    private var alreadyDisposed = false

    override fun close() {
        if (alreadyDisposed)
            return
        alreadyDisposed = true
        instance.value?.setParameterLayoutChangedListener(null)
        coroutineJob.cancel()
        // A load may still be decoding into the player, so release the player only after it.
        if (pluginPlayerDelegate.isInitialized())
            coroutineJob.invokeOnCompletion { pluginPlayer.close() }
    }

    suspend fun instantiatePlugin() {
        if (!manager.connections.any { it.serviceInfo.packageName == pluginInfo.packageName })
            manager.client.connectToPluginService(pluginInfo.packageName)
        val result = manager.client.instantiateNativePlugin(pluginInfo)
        // Register before prepare(): plugins often (re)build their parameter list there.
        result.setParameterLayoutChangedListener {
            mainHandler.post {
                if (alreadyDisposed || instance.value !== result)
                    return@post
                syncParametersFromInstance()
                parameterLayoutRevision.intValue++
            }
        }
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
        val player = pluginPlayer
        // Loading stops playback, so play after it: the first press creates the player, and loads.
        if (isLoadingAudioSource.value)
            playAfterLoading = true
        else
            player.playPreloadedAudio()
    }

    /** Plays [source] on "Play Audio" from now on, and in later sessions, if it can be loaded. */
    fun selectAudioSource(source: AudioSource) {
        // If this creates the player, it loads the current source first, which stays if the new
        // one fails.
        if (!alreadyDisposed)
            loadAudioSource(pluginPlayer, source)
    }

    /** Keeps [audioSourceCandidates] up to date with the "AAP Sample" content that soundpack
     * providers serve, such as the folders shared as "AAP Sample" in the Soundpack Manager. */
    suspend fun collectAudioSourceCandidates() {
        val client = SoundpackClient(manager.context, SoundpackKind.HOST_SAMPLE)
        client.catalogFlow()
            .map { catalog ->
                if (catalog.endpoints.isEmpty())
                    return@map null
                catalog.failures.forEach { Log.w(manager.logTag, "Cannot read ${it.endpoint.authority}: ${it.error.message}") }
                listOf(AudioSource.Bundled) + catalog.packs.flatMap { pack ->
                    runCatching { client.entries(pack) }
                        .onFailure { Log.w(manager.logTag, "Cannot read the entries of ${pack.id}: ${it.message}") }
                        .getOrDefault(emptyList())
                }.map { AudioSource.Soundpack.of(client, it) }
                    .sortedWith(compareBy({ it.location.lowercase() }, { it.displayName.lowercase() }))
            }
            .flowOn(Dispatchers.IO)
            .collect { audioSourceCandidates.value = it }
    }

    private fun loadAudioSource(player: PluginPlayer, source: AudioSource) {
        pendingAudioSourceLoads++
        isLoadingAudioSource.value = true
        coroutineScope.launch {
            try {
                audioSourceMutex.withLock {
                    audioSourceError.value = null
                    val error = withContext(Dispatchers.IO) { loadInto(player, source) }
                    if (error == null)
                        useAudioSource(source)
                    else {
                        Log.w(manager.logTag, error)
                        audioSourceError.value = error
                        // The player has nothing to play yet (the saved sample is gone, for example).
                        if (!playerHasAudio && source != AudioSource.Bundled &&
                            withContext(Dispatchers.IO) { loadInto(player, AudioSource.Bundled) } == null)
                            useAudioSource(AudioSource.Bundled)
                    }
                }
            } finally {
                isLoadingAudioSource.value = --pendingAudioSourceLoads > 0
                if (pendingAudioSourceLoads == 0 && playAfterLoading) {
                    playAfterLoading = false
                    if (!alreadyDisposed)
                        player.playPreloadedAudio()
                }
            }
        }
    }

    // Returns an error message, or null if the player has loaded the source.
    private fun loadInto(player: PluginPlayer, source: AudioSource): String? {
        val bytes = try {
            source.readBytes(manager.context)
        } catch (e: Exception) {
            Log.w(manager.logTag, "Cannot read ${source.fileName}", e)
            return "${source.displayName} cannot be read: ${e.message}"
        } catch (e: OutOfMemoryError) {
            return "${source.displayName} is too large to load"
        }
        return if (player.loadAudioResource(bytes, source.fileName)) null
        else "${source.displayName} cannot be played: it is not a WAV, MP3, Ogg or FLAC file, or it is longer than three minutes"
    }

    private fun useAudioSource(source: AudioSource) {
        playerHasAudio = true
        audioSource.value = source
        AudioSourcePreferences.save(manager.context, source)
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
            // While not processing, the change stays queued in the player, so record it in the
            // native value cache; otherwise syncParametersFromInstance() reverts it (aap-core#220).
            ins.setCachedParameterValue(index, value.toDouble())
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
